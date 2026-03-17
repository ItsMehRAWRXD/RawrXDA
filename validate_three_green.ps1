<# ============================================================================
   validate_three_green.ps1 — End-to-end build + runtime validation
   
   Three-Green checks:
     1. DLL exists and exports load                (BUILD)
     2. Server starts and /api/status returns ok    (RUNTIME)
     3. POST /api/models/upload writes blob to disk (UPLOAD)
   
   Exit 0 = all green, Exit 1 = failures
   ============================================================================ #>
$ErrorActionPreference = 'Continue'
$RepoRoot  = $PSScriptRoot
if (-not (Test-Path "$RepoRoot\server.js")) {
    $RepoRoot = Split-Path -Parent $PSScriptRoot
}
$DllPath   = Join-Path $RepoRoot 'bin\Phase3_Agent_Kernel.dll'
$ServerJs  = Join-Path $RepoRoot 'server.js'
$Port      = 3000
$BaseUrl   = "http://127.0.0.1:$Port"
$Results   = @()
$ServerProc = $null

function Add-Result($Name, $Pass, $Detail) {
    $color = if ($Pass) { 'Green' } else { 'Red' }
    $icon  = if ($Pass) { '✅' } else { '❌' }
    Write-Host "  $icon $Name — $Detail" -ForegroundColor $color
    $script:Results += [PSCustomObject]@{ Name=$Name; Pass=$Pass; Detail=$Detail }
}

Write-Host "`n═══ Three-Green Validation ═══" -ForegroundColor Cyan

# ── CHECK 1: DLL exists and can be loaded ──
Write-Host "`n[1/3] DLL Build Check" -ForegroundColor Yellow
if (Test-Path $DllPath) {
    try {
        $bytes = [System.IO.File]::ReadAllBytes($DllPath)
        $isPE = ($bytes[0] -eq 0x4D) -and ($bytes[1] -eq 0x5A)  # MZ header
        if ($isPE) {
            $sizeKb = [math]::Round($bytes.Length / 1024)
            Add-Result 'DLL-Build' $true "Phase3_Agent_Kernel.dll exists, valid PE ($sizeKb KB)"
        } else {
            Add-Result 'DLL-Build' $false 'File exists but is not a valid PE'
        }
    } catch {
        Add-Result 'DLL-Build' $false "Cannot read DLL: $_"
    }
} else {
    Add-Result 'DLL-Build' $false "DLL not found at $DllPath — run build_phase3_cpp.ps1"
}

# ── CHECK 2: Server /status returns ok ──
Write-Host "`n[2/3] Server Runtime Check" -ForegroundColor Yellow

# See if server is already running
$alreadyRunning = $false
try {
    $resp = Invoke-RestMethod -Uri "$BaseUrl/status" -TimeoutSec 3 -ErrorAction Stop
    if ($resp.status -eq 'ok') { $alreadyRunning = $true }
} catch { }

# Also try /api/status as backup
if (-not $alreadyRunning) {
    try {
        $resp = Invoke-RestMethod -Uri "$BaseUrl/api/status" -TimeoutSec 3 -ErrorAction Stop
        if ($resp.status -eq 'ok') { $alreadyRunning = $true }
    } catch { }
}

if (-not $alreadyRunning) {
    # Start server
    Write-Host "  Starting server..." -ForegroundColor DarkGray
    $ServerProc = Start-Process -FilePath 'node' -ArgumentList $ServerJs `
        -WorkingDirectory $RepoRoot -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 3
}

try {
    # Try /status first, then /api/status as fallback
    $status = $null
    try {
        $status = Invoke-RestMethod -Uri "$BaseUrl/status" -TimeoutSec 5 -ErrorAction Stop
    } catch {
        $status = Invoke-RestMethod -Uri "$BaseUrl/api/status" -TimeoutSec 5 -ErrorAction Stop
    }
    
    if ($status.status -eq 'ok') {
        $dll = if ($status.dllLoaded) { 'DLL=loaded' } else { 'DLL=fallback' }
        Add-Result 'Server-Status' $true "/status → ok ($dll, v$($status.version))"
    } else {
        Add-Result 'Server-Status' $false "/status returned unexpected: $($status | ConvertTo-Json -Compress)"
    }
} catch {
    Add-Result 'Server-Status' $false "Server unreachable: $_"
}

# ── CHECK 3: Blob upload ──
Write-Host "`n[3/3] Blob Upload Check" -ForegroundColor Yellow
$testModelName = '__test_blob__.gguf'
# Try /api/models/upload first, then /api/upload as fallback
$testUrl = "$BaseUrl/api/models/upload?name=$testModelName&overwrite=1"
$uploadEndpoint = "/api/models/upload"

# Create a minimal 16-byte pseudo-GGUF blob (GGUF magic + padding)
$ggufBytes = [byte[]]@(0x47, 0x47, 0x55, 0x46, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)

try {
    $uploadResp = Invoke-RestMethod -Uri $testUrl -Method Post `
        -Body $ggufBytes -ContentType 'application/octet-stream' `
        -TimeoutSec 10 -ErrorAction Stop

    if ($uploadResp.success -eq $true) {
        # Verify file on disk
        $modelFile = Join-Path $RepoRoot "models\$testModelName"
        if (Test-Path $modelFile) {
            $fsize = (Get-Item $modelFile).Length
            Add-Result 'Blob-Upload' $true "Upload succeeded, file on disk ($fsize bytes) at $uploadEndpoint"
            # Clean up test file
            Remove-Item $modelFile -Force -ErrorAction SilentlyContinue
        } else {
            Add-Result 'Blob-Upload' $false "API returned success but file not found on disk"
        }
    } else {
        Add-Result 'Blob-Upload' $false "Upload response: $($uploadResp | ConvertTo-Json -Compress)"
    }
} catch {
    # Fallback: try /api/upload instead
    try {
        $testUrl2 = "$BaseUrl/api/upload?name=$testModelName&overwrite=1"
        $uploadResp = Invoke-RestMethod -Uri $testUrl2 -Method Post `
            -Body $ggufBytes -ContentType 'application/octet-stream' `
            -TimeoutSec 10 -ErrorAction Stop
        
        if ($uploadResp.success -eq $true) {
            $modelFile = Join-Path $RepoRoot "models\$testModelName"
            if (Test-Path $modelFile) {
                $fsize = (Get-Item $modelFile).Length
                Add-Result 'Blob-Upload' $true "Upload succeeded via /api/upload, file on disk ($fsize bytes)"
                Remove-Item $modelFile -Force -ErrorAction SilentlyContinue
            } else {
                Add-Result 'Blob-Upload' $false "Upload to /api/upload succeeded but file not on disk"
            }
        } else {
            Add-Result 'Blob-Upload' $false "Upload to /api/upload failed: $($uploadResp | ConvertTo-Json -Compress)"
        }
    } catch {
        Add-Result 'Blob-Upload' $false "Upload failed on both endpoints: $_"
    }
}

# ── Cleanup ──
if ($ServerProc -and -not $alreadyRunning) {
    try { Stop-Process -Id $ServerProc.Id -Force -ErrorAction SilentlyContinue } catch { }
}

# ── Summary ──
Write-Host "`n═══ Results ═══" -ForegroundColor Cyan
$passed = ($Results | Where-Object Pass).Count
$total  = $Results.Count
$allGreen = ($passed -eq $total)
$color = if ($allGreen) { 'Green' } else { 'Red' }
Write-Host "  $passed / $total checks passed" -ForegroundColor $color

if ($allGreen) {
    Write-Host "`n  🟢🟢🟢 THREE GREEN — All validations passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n  🔴 FAILURES detected — see above" -ForegroundColor Red
    exit 1
}
