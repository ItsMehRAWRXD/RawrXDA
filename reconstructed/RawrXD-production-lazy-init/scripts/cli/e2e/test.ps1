param(
    [string]$CLIPath = "D:\\RawrXD-production-lazy-init\\build\\bin-msvc\\Release\\RawrXD-CLI.exe"
)
$ErrorActionPreference = 'Stop'

function Wait-Health([int]$port){
    for($i=0; $i -lt 50; $i++){
        try {
            $r = Invoke-RestMethod -UseBasicParsing -Uri "http://127.0.0.1:$port/health" -TimeoutSec 2
            if ($r.status -eq 'ok') { return $true }
        } catch {}
        Start-Sleep -Milliseconds 200
    }
    return $false
}

# Start instance 1
$proc1 = Start-Process -FilePath $CLIPath -PassThru
$port1 = 11434 + ($proc1.Id % 100)
Write-Host "Started CLI 1 PID=$($proc1.Id) Port=$port1"
if (-not (Wait-Health $port1)) { throw "CLI 1 health check failed on port $port1" }

# Exercise endpoints on instance 1
$tags1 = Invoke-RestMethod -UseBasicParsing -Uri "http://127.0.0.1:$port1/api/tags" -Method GET
$gen1 = Invoke-RestMethod -UseBasicParsing -Uri "http://127.0.0.1:$port1/api/generate" -Method POST -ContentType 'application/json' -Body '{"model":"test","prompt":"Hello from test 1"}'
$chat1 = Invoke-RestMethod -UseBasicParsing -Uri "http://127.0.0.1:$port1/v1/chat/completions" -Method POST -ContentType 'application/json' -Body '{"model":"test","messages":[{"role":"user","content":"Hi there?"}]}'

Write-Host ("CLI1 OK: tags={0} genBytes={1} chatBytes={2}" -f (($tags1.models | Measure-Object).Count), ([string]$gen1).Length, ([string]$chat1).Length)

# Start instance 2
$proc2 = Start-Process -FilePath $CLIPath -PassThru
$port2 = 11434 + ($proc2.Id % 100)
Write-Host "Started CLI 2 PID=$($proc2.Id) Port=$port2"
if ($port2 -eq $port1) { throw "Port collision: both instances on $port1" }
if (-not (Wait-Health $port2)) { throw "CLI 2 health check failed on port $port2" }

$tags2 = Invoke-RestMethod -UseBasicParsing -Uri "http://127.0.0.1:$port2/api/tags" -Method GET
Write-Host ("CLI2 OK: tags={0}" -f (($tags2.models | Measure-Object).Count))

# Test shell execution (ps)
$outFile = Join-Path $env:TEMP 'rawrxd_cli_ps_test.txt'
$errFile = $outFile + '.err'
if (Test-Path $outFile) { Remove-Item $outFile -Force }
if (Test-Path $errFile) { Remove-Item $errFile -Force }
$psProc = Start-Process -FilePath $CLIPath -ArgumentList 'ps','Write-Output hello-from-ps' -RedirectStandardOutput $outFile -RedirectStandardError $errFile -PassThru
Start-Sleep -Seconds 5
try { Stop-Process -Id $psProc.Id -Force -ErrorAction SilentlyContinue } catch {}
$psOut = if (Test-Path $outFile){ Get-Content $outFile -ErrorAction SilentlyContinue } else { '' }
$psErr = if (Test-Path $errFile){ Get-Content $errFile -ErrorAction SilentlyContinue } else { '' }
$combined = ($psOut + ' ' + $psErr) -join ' '
$psOk = ($combined -match 'hello-from-ps')
Write-Host "Shell(ps) OK: $psOk"

# Cleanup
try { Stop-Process -Id $proc1.Id -Force -ErrorAction SilentlyContinue } catch {}
try { Stop-Process -Id $proc2.Id -Force -ErrorAction SilentlyContinue } catch {}

Write-Host "DONE"
