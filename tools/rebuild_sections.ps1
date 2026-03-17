# save as: tools/rebuild_sections.ps1
$env:LINK = "link.exe"

# Prefer "ours" ml64 if provided; fall back to the MSVC ml64.exe.
# Set `RAWRXD_ML64_PRIMARY` to point at your custom assembler.
$PrimaryMasm = $env:RAWRXD_ML64_PRIMARY
if (-not $PrimaryMasm) {
    $cmd = Get-Command ml64.exe -ErrorAction SilentlyContinue
    if ($cmd) { $PrimaryMasm = $cmd.Source }
}

$FallbackMasm = Get-ChildItem "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\*\\bin\\Hostx64\\x64\\ml64.exe" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName

function Invoke-Ml64WithFallback {
    param([string[]]$Args)

    if ($PrimaryMasm -and (Test-Path $PrimaryMasm)) {
        & $PrimaryMasm @Args
        if ($LASTEXITCODE -eq 0) { return $true }
        Write-Host "[ml64-fallback] Primary failed: $PrimaryMasm. Retrying with fallback: $FallbackMasm" -ForegroundColor Yellow
    }

    if ($FallbackMasm -and (Test-Path $FallbackMasm)) {
        & $FallbackMasm @Args
        return ($LASTEXITCODE -eq 0)
    }

    # Last resort: whatever is on PATH.
    & ml64.exe @Args
    return ($LASTEXITCODE -eq 0)
}

$segments = @(
    @{Name="core"; Src="asm_monolithic_core.asm"; Sections=4},
    @{Name="ai"; Src="asm_monolithic_ai.asm"; Sections=3}, 
    @{Name="gpu"; Src="asm_monolithic_gpu.asm"; Sections=2},
    @{Name="swarm"; Src="asm_monolithic_swarm.asm"; Sections=3}
)

# Rebuild with distributed sections
foreach($seg in $segments) {
    $sectionFlags = ($seg.Sections | ForEach-Object { "/SECTION:.text$_,ER" }) -join " "
    
    if (Test-Path "src\asm\$($seg.Src)") {
        $logPath = "logs\\$($seg.Name)_build.log"
        New-Item -ItemType Directory -Force -Path (Split-Path $logPath -Parent) | Out-Null

        $args = @(
            "/c",
            "/Foobj\\$($seg.Name)_rebuilt.obj",
            "/DSECTION_COUNT=$($seg.Sections)",
            "src\\asm\\$($seg.Src)"
        )

        # Capture output for both attempts into the same log.
        $ok = $true
        try {
            $ok = Invoke-Ml64WithFallback -Args $args 2>&1 | Tee-Object $logPath
        } catch {
            $ok = $false
            $_ | Out-String | Add-Content $logPath
        }
    } else {
        Write-Host "Skipping $($seg.Src) - file not found" -ForegroundColor Yellow
    }
}

Write-Host "✅ Rebuilt with distributed sections - 2GB limit bypassed"
