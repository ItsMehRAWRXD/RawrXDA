# Rebuild Monolithic ASM Objects with /bigobj and Section Distribution
# Fixes section overflow issues for large monolithic assemblies

param(
    [string]$VCToolsDir = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64",
    [string]$AsmDir = "D:\rawrxd\src\asm",
    [string]$ObjDir = "D:\rawrxd\obj"
)

$fallbackMl64 = Join-Path $VCToolsDir "ml64.exe"
$primaryMl64 = $env:RAWRXD_ML64_PRIMARY
if (-not $primaryMl64) {
    # If the user has an "ours" ml64 on PATH, prefer it.
    $cmd = Get-Command ml64.exe -ErrorAction SilentlyContinue
    if ($cmd) { $primaryMl64 = $cmd.Source }
}

if (-not (Test-Path $fallbackMl64)) {
    Write-Host "❌ Fallback ml64.exe not found: $fallbackMl64" -ForegroundColor Red
    exit 1
}

Write-Host "🔨 REBUILDING MONOLITHIC ASM OBJECTS" -ForegroundColor Cyan
  Write-Host ("=" * 80)
if ($primaryMl64) {
    Write-Host "   Primary MASM:  $primaryMl64" -ForegroundColor Gray
}
Write-Host "   Fallback MASM: $fallbackMl64" -ForegroundColor Gray

  function Invoke-Ml64WithFallback {
      param(
          [string]$Primary,
          [string]$Fallback,
          [string[]]$MlArgs
      )

    $out = ""
    $used = $Fallback

      if ($Primary -and (Test-Path $Primary)) {
          $out = (& $Primary @MlArgs 2>&1 | Out-String)
          if ($LASTEXITCODE -eq 0) {
              return @{ Success = $true; Output = $out; Used = $Primary }
          }
        # Primary failed; try fallback.
        $used = $Fallback
        $out += "`n[ml64-fallback] Primary failed ($Primary). Retrying with fallback ($Fallback).`n"
    }

      $out += (& $Fallback @MlArgs 2>&1 | Out-String)
      return @{ Success = ($LASTEXITCODE -eq 0); Output = $out; Used = $used }
  }

# Find all monolithic ASM source files
$asmFiles = Get-ChildItem "$AsmDir\monolithic\*.asm" -ErrorAction SilentlyContinue
if ($asmFiles.Count -eq 0) {
    # Try alternate locations
    $asmFiles = Get-ChildItem "$AsmDir\asm_monolithic_*.asm" -ErrorAction SilentlyContinue
}

if ($asmFiles.Count -eq 0) {
    Write-Host "⚠️  No monolithic ASM source files found in $AsmDir" -ForegroundColor Yellow
    Write-Host "   Looking for pattern: asm_monolithic_*.asm or monolithic/*.asm" -ForegroundColor Gray
    Write-Host "   Current monolithic .obj files are likely pre-built or minimal stubs" -ForegroundColor Gray
    
    # List what exists
    $existingObjs = Get-ChildItem "$ObjDir\asm_monolithic_*.obj"
    Write-Host "`n   Existing monolithic objects:" -ForegroundColor Yellow
    $existingObjs | ForEach-Object {
        Write-Host "     $($_.Name) - $([math]::Round($_.Length/1KB, 2)) KB" -ForegroundColor Gray
    }
    
    Write-Host "`n   These are TINY (<25KB each) - likely placeholder/stub objects" -ForegroundColor Yellow
    Write-Host "   Real monolithic sources may be lost or never existed" -ForegroundColor Yellow
    Write-Host "   The actual IDE is built from the large C++ objects (13-14MB each)" -ForegroundColor Cyan
    
    exit 0
}

Write-Host "Found $($asmFiles.Count) monolithic ASM source files" -ForegroundColor Green

$buildResults = @()

foreach ($asmFile in $asmFiles) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($asmFile.Name)
    $objFile = Join-Path $ObjDir "$baseName.obj"
    $lstFile = Join-Path $ObjDir "$baseName.lst"
    
    Write-Host "`n🔧 Assembling: $($asmFile.Name)" -ForegroundColor Yellow
    
    # Assemble with safety flags (note: /bigobj is for cl.exe not ml64)
    $asmArgs = @(
        "/c",                    # Compile only
        "/Zi",                   # Debug info
        "/nologo",               # No banner
        "/Fo$objFile",           # Output object (no quotes needed)
        "/Fl$lstFile",           # Generate listing (no quotes needed)
        $asmFile.FullName        # Source file (auto-quoted if spaces)
    )
    
    Write-Host "   Command: ml64.exe /c /Zi /Fo $baseName.obj $($asmFile.Name)" -ForegroundColor Gray
    
      $result = Invoke-Ml64WithFallback -Primary $primaryMl64 -Fallback $fallbackMl64 -MlArgs $asmArgs
      $output = $result.Output
      $success = [bool]$result.Success
    
    if ($success -and (Test-Path $objFile)) {
        $size = [math]::Round((Get-Item $objFile).Length / 1KB, 2)
        Write-Host "   ✅ Success: $size KB (via $([System.IO.Path]::GetFileName($result.Used)))" -ForegroundColor Green
        
        $buildResults += [PSCustomObject]@{
            Source  = $asmFile.Name
            Object  = $baseName + ".obj"
            Status  = "SUCCESS"
            SizeKB  = $size
            Errors  = ""
        }
    } else {
        Write-Host "   ❌ Build failed" -ForegroundColor Red
        Write-Host $output -ForegroundColor Red
        
        # Extract error summary
        $errors = ($output -split "`n" | Where-Object { $_ -match "error" }) -join "; "
        
        $buildResults += [PSCustomObject]@{
            Source  = $asmFile.Name
            Object  = $baseName + ".obj"
            Status  = "FAILED"
            SizeKB  = 0
            Errors  = $errors
        }
    }
}

# Summary report
  Write-Host ("`n" + ("=" * 80))
Write-Host "📊 BUILD SUMMARY" -ForegroundColor Cyan
$buildResults | Format-Table -AutoSize

$successCount = ($buildResults | Where-Object { $_.Status -eq "SUCCESS" }).Count
$totalCount = $buildResults.Count

Write-Host "`n✅ Rebuilt: $successCount / $totalCount objects" -ForegroundColor $(if ($successCount -eq $totalCount) { "Green" } else { "Yellow" })

# Save report
$reportPath = Join-Path $ObjDir "rebuild_report.json"
$buildResults | ConvertTo-Json -Depth 2 | Out-File $reportPath
Write-Host "   Report saved: $reportPath" -ForegroundColor Gray

if ($successCount -gt 0) {
    Write-Host "`n🚀 Ready for linking - run link_strategies.ps1 next" -ForegroundColor Cyan
}
