# RawrXD.Win32Deployment.psm1
# PowerShell scaffolding for Win32 IDE build/deployment

function Test-RawrXDWin32Prereqs {
    $checks = @(
        @{ Name = 'CMake'; Command = 'cmake' },
        @{ Name = 'MinGW Make'; Command = 'mingw32-make' }
    )
    $results = @()
    foreach ($check in $checks) {
        $exists = Get-Command $check.Command -ErrorAction SilentlyContinue
        $results += [ordered]@{ Component = $check.Name; Available = [bool]$exists; Command = $check.Command }
    }
    return $results
}

function Invoke-RawrXDWin32Build {
    param(
        [string]$SourcePath = (Get-RawrXDRootPath),
        [string]$BuildPath = (Join-Path (Get-RawrXDRootPath) 'build-win32'),
        [string]$BuildType = 'Release',
        [string]$Generator = 'MinGW Makefiles',
        [switch]$MASM,
        [switch]$NASM,
        [switch]$DryRun
    )

    if ($DryRun) {
        $steps = @(
            "cmake -S `"$SourcePath`" -B `"$BuildPath`" -G `"$Generator`" -DBUILD_TESTS=ON",
            "cmake --build `"$BuildPath`" --config $BuildType"
        )
        return @{ Success = $true; Steps = $steps }
    }

    # Actual build execution
    try {
        if (-not (Test-Path $BuildPath)) {
            New-Item -ItemType Directory -Path $BuildPath -Force | Out-Null
        }

        $cmakeFile = Join-Path $SourcePath 'CMakeLists.txt'
        if (-not (Test-Path $cmakeFile)) {
            return @{ Success = $false; Error = 'CMakeLists.txt not found' }
        }

        Push-Location $BuildPath
        try {
            Write-Host "[Win32Build] Configuring..." -ForegroundColor Cyan
            $configArgs = @('-S', $SourcePath, '-B', '.', '-G', $Generator, "-DCMAKE_BUILD_TYPE=$BuildType")
            
            if ($MASM) {
                $masmPath = 'C:\masm32\bin\ml.exe'
                if (Test-Path $masmPath) {
                    $configArgs += '-DENABLE_MASM=ON', "-DCMAKE_ASM_MASM_COMPILER=$masmPath"
                }
            }
            
            if ($NASM) {
                $nasmPath = 'C:\nasm\nasm.exe'
                if (Test-Path $nasmPath) {
                    $configArgs += '-DENABLE_NASM=ON', "-DCMAKE_ASM_NASM_COMPILER=$nasmPath"
                }
            }
            
            & cmake $configArgs
            if ($LASTEXITCODE -ne 0) {
                return @{ Success = $false; Error = "CMake config failed: $LASTEXITCODE" }
            }

            Write-Host "[Win32Build] Building..." -ForegroundColor Cyan
            & cmake --build . --config $BuildType --parallel
            if ($LASTEXITCODE -ne 0) {
                return @{ Success = $false; Error = "Build failed: $LASTEXITCODE" }
            }

            Write-Host "[Win32Build] Completed!" -ForegroundColor Green
            return @{ Success = $true; BuildPath = $BuildPath; BuildType = $BuildType }
        } finally {
            Pop-Location
        }
    } catch {
        return @{ Success = $false; Error = $_.Exception.Message }
    }
}

function Initialize-RawrXDPackage {
    param(
        [string]$PackageName = "RawrXD-Production",
        [string]$Version = "1.0.0"
    )
    
    $outDir = Join-Path (Get-RawrXDRootPath) "dist\$PackageName-$Version"
    if (Test-Path $outDir) { Remove-Item $outDir -Recurse -Force }
    New-Item -Path $outDir -ItemType Directory -Force | Out-Null
    
    # Copy core modules
    $modulesDir = New-Item -Path (Join-Path $outDir "modules") -ItemType Directory -Force
    Get-ChildItem (Get-RawrXDRootPath) -Filter "RawrXD.*.psm1" | Copy-Item -Destination $modulesDir
    
    # Copy binaries
    $binDir = New-Item -Path (Join-Path $outDir "bin") -ItemType Directory -Force
    if (Test-Path (Join-Path (Get-RawrXDRootPath) "RawrXD-ModelLoader.exe")) {
        Copy-Item (Join-Path (Get-RawrXDRootPath) "RawrXD-ModelLoader.exe") -Destination $binDir
    }
    
    # Create launcher
    $launcherContent = @"
@echo off
powershell.exe -ExecutionPolicy Bypass -Command "Import-Module './modules/RawrXD.Core.psm1'; Start-RawrXDAutonomousLoop -Goal '%*'"
"@
    $launcherContent | Set-Content (Join-Path $outDir "run.bat")
    
    Write-Host "Package created at: $outDir" -ForegroundColor Green
    return $outDir
}

Export-ModuleMember -Function Test-RawrXDWin32Prereqs, Invoke-RawrXDWin32Build, Initialize-RawrXDPackage
