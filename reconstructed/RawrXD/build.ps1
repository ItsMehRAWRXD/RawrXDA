# build.ps1 - RXUC Compiler Builder
# Automatically discovers MSVC BuildTools and compiles

param(
    [string]$SourceFile = "terraform.asm",
    [string]$OutputName = "terraform.exe"
)

function Find-VSTools {
    $vsPaths = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC"
    )
    
    foreach ($base in $vsPaths) {
        if (Test-Path $base) {
            $latest = Get-ChildItem $base | Sort-Object Name -Descending | Select-Object -First 1
            if ($latest) {
                $ml64 = Join-Path $latest.FullName "bin\Hostx64\x64\ml64.exe"
                $link = Join-Path $latest.FullName "bin\Hostx64\x64\link.exe"
                
                if ((Test-Path $ml64) -and (Test-Path $link)) {
                    return @{ ML64 = $ml64; LINK = $link; LIB = (Join-Path $latest.FullName "lib\x64") }
                }
            }
        }
    }
    
    # Try vswhere.exe
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $installPath = & $vsWhere -latest -property installationPath
        if ($installPath) {
            $ml64 = Join-Path $installPath "VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe"
            $link = Join-Path $installPath "VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe"
            $ml64Path = Resolve-Path $ml64 | Select-Object -First 1
            $linkPath = Resolve-Path $link | Select-Object -First 1
            
            if ($ml64Path -and $linkPath) {
                $libPath = $ml64Path.Path -replace 'bin\\Hostx64\\x64\\ml64\.exe','lib\\x64'
                return @{ ML64 = $ml64Path.Path; LINK = $linkPath.Path; LIB = $libPath }
            }
        }
    }
    
    throw "MSVC BuildTools not found! Install from: https://aka.ms/vs/17/release/vs_BuildTools.exe"
}

try {
    Write-Host "[*] Locating MSVC BuildTools..." -ForegroundColor Cyan
    $tools = Find-VSTools
    Write-Host "[+] Found ml64.exe at: $($tools.ML64)" -ForegroundColor Green
    
    # Find Windows SDK lib path for kernel32.lib
    $sdkLib = ""
    $sdkBase = "${env:ProgramFiles(x86)}\Windows Kits\10\Lib"
    if (Test-Path $sdkBase) {
        $sdkVer = Get-ChildItem $sdkBase | Where-Object { $_.Name -match '^\d' } | Sort-Object Name -Descending | Select-Object -First 1
        if ($sdkVer) {
            $sdkLib = Join-Path $sdkVer.FullName "um\x64"
        }
    }
    
    # Assemble
    Write-Host "[*] Assembling $SourceFile..." -ForegroundColor Cyan
    $objFile = [System.IO.Path]::ChangeExtension($SourceFile, ".obj")
    
    $asmArgs = @("/c", "/nologo", "/Zi", $SourceFile)
    $proc = Start-Process -FilePath $tools.ML64 -ArgumentList $asmArgs -Wait -NoNewWindow -PassThru -RedirectStandardError "ml_err.txt" -RedirectStandardOutput "ml_out.txt"
    
    Get-Content "ml_out.txt" -ErrorAction SilentlyContinue | Write-Host
    Get-Content "ml_err.txt" -ErrorAction SilentlyContinue | Write-Host -ForegroundColor Yellow
    
    if ($proc.ExitCode -ne 0) {
        throw "Assembly failed with exit code $($proc.ExitCode)"
    }
    
    if (-not (Test-Path $objFile)) {
        throw "Object file not created"
    }
    Write-Host "[+] Assembly succeeded: $objFile" -ForegroundColor Green
    
    # Link
    Write-Host "[*] Linking $OutputName..." -ForegroundColor Cyan
    $linkArgs = @("/ENTRY:_start", "/SUBSYSTEM:CONSOLE", "/OUT:$OutputName", "/FIXED:NO", $objFile, "kernel32.lib")
    
    # Add library paths
    if ($tools.LIB -and (Test-Path $tools.LIB)) {
        $linkArgs += "/LIBPATH:$($tools.LIB)"
    }
    if ($sdkLib -and (Test-Path $sdkLib)) {
        $linkArgs += "/LIBPATH:$sdkLib"
    }
    
    $proc = Start-Process -FilePath $tools.LINK -ArgumentList $linkArgs -Wait -NoNewWindow -PassThru -RedirectStandardError "link_err.txt" -RedirectStandardOutput "link_out.txt"
    
    Get-Content "link_out.txt" -ErrorAction SilentlyContinue | Write-Host
    Get-Content "link_err.txt" -ErrorAction SilentlyContinue | Write-Host -ForegroundColor Yellow
    
    if ($proc.ExitCode -ne 0) {
        throw "Link failed with exit code $($proc.ExitCode)"
    }
    
    if (Test-Path $OutputName) {
        $size = (Get-Item $OutputName).Length
        Write-Host "[+] Success: $OutputName ($size bytes)" -ForegroundColor Green
    } else {
        throw "Output file not created"
    }
    
} catch {
    Write-Host "[!] Error: $_" -ForegroundColor Red
    exit 1
}
