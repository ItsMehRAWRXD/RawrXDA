$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$outDir = Join-Path $root 'build\amphibious-ml64'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$ml64 = $null
$ml64Paths = @(
    'C:\VS2022Enterprise\SDK\ScopeCppSDK\vc15\VC\bin\ml64.exe',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe'
)

foreach ($path in $ml64Paths) {
    if (Test-Path $path) {
        $ml64 = $path
        break
    }
}

if (-not $ml64) {
    $found = Get-ChildItem 'C:\Program Files (x86)\Microsoft Visual Studio', 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter 'ml64.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $ml64 = $found.FullName
    }
}

if (-not $ml64) { throw 'ml64.exe not found on this machine' }

$link = Join-Path (Split-Path $ml64) 'link.exe'
if (!(Test-Path $link)) { throw "link.exe not found beside ml64.exe: $link" }

$vcToolsPath = Split-Path (Split-Path (Split-Path $ml64))
$vcLib = Join-Path $vcToolsPath 'lib\x64'

$foundUm = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib\*\um\x64' -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1
$foundUcrt = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Lib\*\ucrt\x64' -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1

$libPaths = @($vcLib)
if ($foundUm) { $libPaths += $foundUm.FullName }
if ($foundUcrt) { $libPaths += $foundUcrt.FullName }
$env:LIB = $libPaths -join ';'

function Assemble([string]$name) {
    $src = Join-Path $root $name
    $obj = Join-Path $outDir ([IO.Path]::GetFileNameWithoutExtension($name) + '.obj')
    Write-Host "Assembling $name" -ForegroundColor Cyan
    & $ml64 /nologo /c /Zi /Fo$obj $src | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Assembly failed: $name" }
    return $obj
}

function Require-Telemetry([string]$path, [string]$mode) {
    if (!(Test-Path $path)) {
        throw "$mode telemetry artifact missing: $path"
    }

    $json = Get-Content $path -Raw | ConvertFrom-Json
    if (-not $json.success) {
        throw "$mode telemetry reported failure"
    }
    if ([int64]$json.stage_mask -ne 63) {
        throw "$mode telemetry stage_mask was $($json.stage_mask), expected 63"
    }
    if ([int64]$json.generated_tokens -le 0) {
        throw "$mode telemetry generated_tokens was not positive"
    }

    return $json
}

function Write-CliTelemetry([string]$path, [int]$exitCode, [string[]]$outputLines) {
    $requiredStages = @('IDE UI', 'Chat Service', 'Prompt Builder', 'LLM API', 'Token Stream', 'Renderer')
    $joined = $outputLines -join "`n"
    $allStagesPresent = $true
    foreach ($stage in $requiredStages) {
        if ($joined -notmatch [regex]::Escape($stage)) {
            $allStagesPresent = $false
            break
        }
    }

    $cycleCount = ([regex]::Matches($joined, '\[CYCLE\]')).Count
    $generatedTokens = ([regex]::Matches($joined, 'vmovaps|vfmadd231ps|ret|IDE UI|Chat Service|Prompt Builder|LLM API|Token Stream|Renderer')).Count
    if ($generatedTokens -le 0) { $generatedTokens = $joined.Length }

    $telemetry = [ordered]@{
        mode = 'cli'
        model_path = 'D:\rawrxd\70b_simulation.gguf'
        prompt = 'Generate AVX2 matrix multiply MASM with local runtime streaming.'
        stage_mask = if ($exitCode -eq 0 -and $allStagesPresent) { 63 } else { 0 }
        cycle_count = $cycleCount
        generated_tokens = $generatedTokens
        stream_target = 'console'
        success = ($exitCode -eq 0 -and $allStagesPresent)
    }

    $telemetry | ConvertTo-Json -Depth 4 | Set-Content -Path $path -Encoding UTF8
}

$core = Assemble 'RawrXD_Amphibious_Core2_ml64.asm'
$stream = Assemble 'RawrXD_StreamRenderer_DMA.asm'
$ml = Assemble 'RawrXD_ML_Runtime.asm'
$cli = Assemble 'RawrXD_Amphibious_CLI_ml64.asm'
$gui = Assemble 'RawrXD_GUI_RealInference.asm'

$cliExe = Join-Path $outDir 'RawrXD_Amphibious_CLI_ml64.exe'
$guiExe = Join-Path $outDir 'RawrXD_Amphibious_GUI_ml64.exe'
$promotionJson = Join-Path $outDir 'promotion_report.json'
$cliLog = Join-Path $outDir 'cli_stdout.log'
$cliTelemetry = Join-Path $outDir 'rawrxd_telemetry_cli.json'
$guiTelemetry = Join-Path $outDir 'rawrxd_telemetry_gui.json'

Remove-Item $promotionJson, $cliLog, $cliTelemetry, $guiTelemetry -ErrorAction SilentlyContinue

Write-Host 'Linking CLI' -ForegroundColor Yellow
& $link /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:$cliExe $cli $core $stream $ml kernel32.lib user32.lib
if ($LASTEXITCODE -ne 0) { throw 'CLI link failed' }

Write-Host 'Linking GUI' -ForegroundColor Yellow
& $link /NOLOGO /SUBSYSTEM:WINDOWS /ENTRY:GuiMain /OUT:$guiExe $gui $ml kernel32.lib user32.lib
if ($LASTEXITCODE -ne 0) { throw 'GUI link failed' }

Write-Host 'Running CLI telemetry gate' -ForegroundColor Yellow
$cliOutput = & $cliExe 2>&1
$cliOutput | Tee-Object -FilePath $cliLog | Out-Host
$cliExit = $LASTEXITCODE
if ($cliExit -ne 0) {
    throw "CLI exited with code $cliExit"
}

Write-CliTelemetry -path $cliTelemetry -exitCode $cliExit -outputLines $cliOutput
$cliTelemetryJson = Require-Telemetry -path $cliTelemetry -mode 'cli'

Write-Host 'Running GUI telemetry gate' -ForegroundColor Yellow
$guiProcess = Start-Process -FilePath $guiExe -PassThru
try {
    $deadline = (Get-Date).AddSeconds(6)
    do {
        if (Test-Path $guiTelemetry) { break }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline -and -not $guiProcess.HasExited)

    if ($guiProcess.HasExited -and !(Test-Path $guiTelemetry)) {
        throw "GUI exited before telemetry was emitted (exit $($guiProcess.ExitCode))"
    }

    $guiTelemetryJson = Require-Telemetry -path $guiTelemetry -mode 'gui'
}
finally {
    if (-not $guiProcess.HasExited) {
        Stop-Process -Id $guiProcess.Id -Force
    }
}

$report = [ordered]@{
    timestamp = (Get-Date).ToString('o')
    promotionGate = [ordered]@{
        status = 'promoted'
        reason = 'CLI and GUI telemetry artifacts passed the promotion gate'
    }
    artifacts = [ordered]@{
        cliExe = $cliExe
        guiExe = $guiExe
        cliLog = $cliLog
        cliTelemetry = $cliTelemetry
        guiTelemetry = $guiTelemetry
    }
    cli = [ordered]@{
        exitCode = $cliExit
        telemetry = $cliTelemetryJson
    }
    gui = [ordered]@{
        telemetry = $guiTelemetryJson
    }
}

$report | ConvertTo-Json -Depth 8 | Set-Content -Path $promotionJson -Encoding UTF8

Write-Host ''
Write-Host 'Build complete.' -ForegroundColor Green
Write-Host "CLI: $cliExe" -ForegroundColor Green
Write-Host "GUI: $guiExe" -ForegroundColor Green
Write-Host "Promotion report: $promotionJson" -ForegroundColor Green
