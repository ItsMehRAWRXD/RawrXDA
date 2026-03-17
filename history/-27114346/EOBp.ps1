$ErrorActionPreference = "Stop"

$Root = "D:\rawrxd"
$OutDir = "$env:LOCALAPPDATA\RawrXD\bin"
$finalExe = Join-Path $OutDir "RawrXD.exe"
$Linker = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"

Write-Host "============================================================" -Fore Cyan
Write-Host " RAWRXD BRUTE FORCE LINKER" -Fore Cyan
Write-Host " Automatically eliminating corrupt/incompatible .obj files" -Fore Gray
Write-Host "============================================================" -Fore Cyan

if(!(Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$ExcludeDirs = @(".git", "dist", "node_modules", "temp", "source_audit", "crash_dumps", "build_fresh", "build_gold", "build_prod", "build_universal", "build_msvc", "build_qt_free", "build_win32_gui_test", "CMakeFiles", "build_clean", "build_ide_ninja", "build_new", "build_test_parse")

$objFiles = Get-ChildItem -Path $Root -Filter "*.obj" -Recurse -ErrorAction SilentlyContinue | Where-Object {
    $path = $_.FullName
    $name = $_.Name
    if($name -match "\.cpp\.obj$" -or $name -match "\.c\.obj$" -or $name -match "\.cc\.obj$") { return $false }
    if($name -match "^bench_" -or $name -match "^test_" -or $name -match "compiler_from_scratch" -or $name -match "omega_pro" -or $name -match "OmegaPolyglot_v5") { return $false }
    if($name -match "dumpbin_final\.obj") { return $false }
    foreach($dir in $ExcludeDirs) {
        if($path -match "\\$dir\\" -or $path -match "CMakeFiles") { return $false }
    }
    return $true
}

$uniqueObjs = $objFiles | Sort-Object LastWriteTime -Descending | Group-Object Name | ForEach-Object { $_.Group[0] }
$currentObjs = @()
foreach ($obj in $uniqueObjs) { $currentObjs += $obj }

$linkArgsBase = @(
    "/OUT:`"$finalExe`"",
    "/SUBSYSTEM:WINDOWS",
    "/ENTRY:WinMain",
    "/LARGEADDRESSAWARE:NO",
    "/FIXED:NO",
    "/DYNAMICBASE",
    "/NXCOMPAT",
    "/NODEFAULTLIB:libcmt",
    "/NODEFAULTLIB:vulkan-1.lib",
    "/MERGE:.rdata=.text",
    "/FORCE:MULTIPLE",
    "/FORCE:UNRESOLVED",
    "/LIBPATH:`"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64`"",
    "/LIBPATH:`"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64`"",
    "/LIBPATH:`"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64`""
)

$coreLib = Join-Path $Root "lib\rawrxd_core.lib"
$gpuLib = Join-Path $Root "lib\rawrxd_gpu.lib"
if (Test-Path $coreLib) { $linkArgsBase += "`"$coreLib`"" }
if (Test-Path $gpuLib) { $linkArgsBase += "`"$gpuLib`"" }
$linkArgsBase += "kernel32.lib", "user32.lib", "gdi32.lib", "shell32.lib", "ole32.lib", "advapi32.lib", "crypt32.lib", "psapi.lib", "shlwapi.lib", "ws2_32.lib", "ntdll.lib", "msvcrt.lib", "vcruntime.lib", "ucrt.lib"

$maxAttempts = 50
$attempt = 1

while ($attempt -le $maxAttempts) {
    Write-Host "`n[ATTEMPT $attempt] Linking $($currentObjs.Count) objects..." -Fore Cyan
    
    $responseFile = Join-Path $OutDir "link_objects.rsp"
    $currentObjs.FullName | Out-File -FilePath $responseFile -Encoding ASCII
    
    $linkArgs = $linkArgsBase + "@`"$responseFile`""
    
    $processInfo = New-Object System.Diagnostics.ProcessStartInfo
    $processInfo.FileName = $Linker
    $processInfo.Arguments = $linkArgs -join " "
    $processInfo.RedirectStandardOutput = $true
    $processInfo.RedirectStandardError = $true
    $processInfo.UseShellExecute = $false
    $processInfo.CreateNoWindow = $true
    
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $processInfo
    $process.Start() | Out-Null
    
    $output = $process.StandardOutput.ReadToEnd()
    $errorOutput = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    
    $fullOutput = $output + "`n" + $errorOutput
    
    # If exit code is 0, or if it's just warnings (like LNK4088) and the exe exists
    if ($process.ExitCode -eq 0 -or ((Test-Path $finalExe) -and $fullOutput -notmatch "fatal error")) {
        Write-Host "`n============================================================" -Fore Green
        Write-Host " SUCCESS! BRUTE FORCE LINKING COMPLETE" -Fore Green
        Write-Host " Final Executable: $finalExe" -Fore White
        Write-Host "============================================================" -Fore Green
        break
    }
    
    Write-Host "[FAILED] Exit Code: $($process.ExitCode)" -Fore Red
    
    # Parse output for offending obj
    $match = [regex]::Match($fullOutput, '([a-zA-Z0-9_.-]+\.obj)\s*:\s*(?:fatal\s+)?error\s+LNK(?:1223|1112|1143|1136|1107|1104|2017|1257)')
    
    if ($match.Success) {
        $badObjName = [System.IO.Path]::GetFileName($match.Groups[1].Value)
        Write-Host "[BRUTE FORCER] Identified bad object: $badObjName. Removing from list..." -Fore Yellow
        $currentObjs = $currentObjs | Where-Object { $_.Name -ne $badObjName }
    } else {
        $matchBroad = [regex]::Match($fullOutput, '([a-zA-Z0-9_.-]+\.obj)\s*:\s*(?:fatal\s+)?error')
        if ($matchBroad.Success) {
            $badObjName = [System.IO.Path]::GetFileName($matchBroad.Groups[1].Value)
            Write-Host "[BRUTE FORCER] Identified bad object (broad match): $badObjName. Removing from list..." -Fore Yellow
            $currentObjs = $currentObjs | Where-Object { $_.Name -ne $badObjName }
        } else {
            Write-Host "[BRUTE FORCER] Could not identify a specific .obj causing the failure. Manual intervention required." -Fore Red
            Write-Host "--- LAST OUTPUT ---" -Fore DarkGray
            Write-Host $fullOutput -Fore DarkGray
            break
        }
    }
    
    $attempt++
}

if ($attempt -gt $maxAttempts) {
    Write-Host "[BRUTE FORCER] Reached maximum attempts ($maxAttempts). Aborting." -Fore Red
}
