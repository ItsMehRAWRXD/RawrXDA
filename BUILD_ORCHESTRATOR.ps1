#requires -Version 7.0
<#
.SYNOPSIS
    RawrXD IDE - BUILD ORCHESTRATOR
    
.DESCRIPTION
    Intelligently runs the best build script based on your situation.
    No decision paralysis, just one command to build it all.
#>
param(
    [ValidateSet('quick', 'dev', 'production', 'test', 'interactive', 'compilers', 'ide', 'debug')]
    [string]$Mode = 'quick',
    
    [switch]$SkipAnalysis
)

$ProjectRoot = 'D:\rawrxd'
Set-Location $ProjectRoot

function Banner {
    Write-Host @"
╔═══════════════════════════════════════════════════════════════════════╗
║  RawrXD IDE - UNIFIED BUILD ORCHESTRATOR                             ║
║  Zero scaffolding. Real output. Fast iteration.                       ║
╚═══════════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Magenta
}

function AnalyzeBuildState {
    if ($SkipAnalysis) { return @{ NeedsCompilers = $true; NeedsIDE = $true } }
    
    $state = @{
        CompilersBuilt = (Get-ChildItem 'compilers' -Filter '*.exe' -ErrorAction SilentlyContinue | Measure-Object).Count -gt 50
        IDEBuilt = (Get-ChildItem 'build' -Recurse -Filter '*.exe' -ErrorAction SilentlyContinue | Where-Object { $_.Name -in 'RawrXD-Win32IDE.exe','rawrxd.exe' } | Measure-Object).Count -gt 0
        DesktopReady = (Get-ChildItem 'dist' -Filter '*.ps1' -ErrorAction SilentlyContinue | Measure-Object).Count -gt 0
    }
    
    return $state
}

function SelectMode {
    if ($Mode -ne 'interactive') { return }
    
    Clear-Host
    Write-Host @"
SELECT BUILD MODE:

1. QUICK (2-5 min)
   ✓ Fast iteration during development
   ✓ Builds essentials, skips validation
   ✓ Good for: Testing your changes

2. DEV (5-15 min)
   ✓ Full build but no deep analysis
   ✓ Tests after building
   ✓ Good for: Feature development

3. PRODUCTION (15-30 min)
   ✓ Complete validation & testing
   ✓ Generates build report
   ✓ Good for: Release builds

4. TEST (varies)
   ✓ Only runs tests on existing build
   ✓ Good for: Validation only

5. DEBUG (15-25 min)
   ✓ Verbose output, stops on first error
   ✓ Good for: Fixing build issues

6. COMPILERS ONLY (5-10 min)
   ✓ Just rebuild language compilers
   ✓ Good for: Compiler development

7. IDE ONLY (10-15 min)
   ✓ Just rebuild the IDE
   ✓ Good for: IDE feature development

"@ -ForegroundColor Cyan
    
    $choice = Read-Host "Select mode (1-7)"
    $modes = @('quick', 'dev', 'production', 'test', 'debug', 'compilers', 'ide')
    if ($choice -ge 1 -and $choice -le 7) {
        $Mode = $modes[$choice - 1]
    }
}

# ═══════════════════════════════════════════════════════════════════════════════

Banner

Write-Host "`n[INFO] Analyzing build state..." -ForegroundColor Cyan
$state = AnalyzeBuildState

Write-Host "[STATE] Compilers: $($state.CompilersBuilt)" -ForegroundColor Gray
Write-Host "[STATE] IDE: $($state.IDEBuilt)" -ForegroundColor Gray
Write-Host "[STATE] Desktop: $($state.DesktopReady)" -ForegroundColor Gray

Write-Host "`n[INFO] Mode selected: $Mode" -ForegroundColor Cyan

# Execute the appropriate build
$startTime = Get-Date

switch ($Mode) {
    'quick' {
        Write-Host "`n▶ QUICK BUILD - Essentials only" -ForegroundColor Green
        Write-Host "  Time estimate: 2-5 minutes`n" -ForegroundColor Gray
        & .\BUILD_IDE_FAST.ps1 -What all -Test
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  → Building RawrXD-ModelAnalysis target..." -ForegroundColor Yellow
            Push-Location "$ProjectRoot\build"
            try {
                cmake --build . --target RawrXD-ModelAnalysis 2>&1 | Out-Host
                if ($LASTEXITCODE -eq 0) {
                    Write-Host "  ✓ RawrXD-ModelAnalysis built" -ForegroundColor Green
                } else {
                    Write-Host "  ! RawrXD-ModelAnalysis build returned exit code $LASTEXITCODE" -ForegroundColor Yellow
                }
            } finally {
                Pop-Location
            }
        }
    }
    
    'dev' {
        Write-Host "`n▶ DEV BUILD - Full build with testing" -ForegroundColor Green
        Write-Host "  Time estimate: 5-15 minutes`n" -ForegroundColor Gray
        & .\BUILD_IDE_PRODUCTION.ps1 -Target Full -Config Release -Test
    }
    
    'production' {
        Write-Host "`n▶ PRODUCTION BUILD - Full validation" -ForegroundColor Green
        Write-Host "  Time estimate: 15-30 minutes`n" -ForegroundColor Gray
        & .\BUILD_IDE_PRODUCTION.ps1 -Target Full -Config Release -Clean -Test -Verbose
    }
    
    'test' {
        Write-Host "`n▶ TEST MODE - Validate existing build" -ForegroundColor Green
        if ($state.IDEBuilt) {
            $ide = Get-ChildItem 'build' -Recurse -Filter '*.exe' -ErrorAction SilentlyContinue | Where-Object { $_.Name -in 'RawrXD-Win32IDE.exe','rawrxd.exe' } | Select-Object -First 1
            Write-Host "  Found IDE: $($ide.FullName)" -ForegroundColor Green
            Write-Host "  To launch: & '$($ide.FullName)'" -ForegroundColor Cyan
        } else {
            Write-Host "  IDE not built yet. Run 'quick' or 'dev' mode first." -ForegroundColor Yellow
        }
    }
    
    'debug' {
        Write-Host "`n▶ DEBUG BUILD - Verbose output, stop on error" -ForegroundColor Yellow
        Write-Host "  Time estimate: 15-25 minutes`n" -ForegroundColor Gray
        & .\BUILD_IDE_EXECUTOR.ps1 -Verbose -CleanFirst
    }
    
    'compilers' {
        Write-Host "`n▶ COMPILER BUILD - Language compilers only" -ForegroundColor Magenta
        Write-Host "  Time estimate: 5-10 minutes`n" -ForegroundColor Gray
        & .\BUILD_IDE_FAST.ps1 -What compilers -Test -Clean
    }
    
    'ide' {
        Write-Host "`n▶ IDE BUILD - IDE only (assumes compilers exist)" -ForegroundColor Cyan
        Write-Host "  Time estimate: 10-15 minutes`n" -ForegroundColor Gray
        & .\BUILD_IDE_FAST.ps1 -What ide -Test
    }
}

$duration = (Get-Date) - $startTime

Write-Host @"

╔═══════════════════════════════════════════════════════════════════════╗
║  BUILD COMPLETE                                                       ║
╚═══════════════════════════════════════════════════════════════════════╝

TIME TAKEN: $($duration.TotalMinutes.ToString("0.0")) minutes

OUTPUT LOCATIONS:
  Compilers  → ./compilers/*.exe
  IDE        → ./build/bin/RawrXD.exe
  Distribution → ./dist/*

NEXT STEPS:
  1. Check output: dir dist/
  2. Test IDE: .\BUILD_ORCHESTRATOR.ps1 -Mode test
  3. Run IDE: [IDE exe path]
  4. Deploy: Copy dist/* to target

QUICK COMMANDS:
  .\BUILD_ORCHESTRATOR.ps1 -Mode quick    # Fast iteration
  .\BUILD_ORCHESTRATOR.ps1 -Mode dev      # Full build
  .\BUILD_ORCHESTRATOR.ps1 -Mode test     # Validate existing
  .\BUILD_ORCHESTRATOR.ps1 -Mode debug    # Verbose troubleshooting

"@ -ForegroundColor Green

# Verify outputs exist
$artifacts = @(
    @{ Path = 'compilers'; Type = 'Compilers' },
    @{ Path = 'build'; Type = 'IDE Build' },
    @{ Path = 'dist'; Type = 'Distribution' }
)

Write-Host "ARTIFACT SUMMARY:" -ForegroundColor Cyan
foreach ($artifact in $artifacts) {
    if (Test-Path $artifact.Path) {
        $files = Get-ChildItem $artifact.Path -Recurse -File | Measure-Object
        $exes = Get-ChildItem $artifact.Path -Filter '*.exe' -Recurse -ErrorAction SilentlyContinue | Measure-Object
        Write-Host "  ✓ $($artifact.Type): $($files.Count) files, $($exes.Count) executables" -ForegroundColor Green
    }
}
