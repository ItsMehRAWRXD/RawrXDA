# RawrXD Production Assembly Toolchain - Build Script
# Compiles PE Generator, Assembler Loop, and x64 Encoder

$ErrorActionPreference = "Continue"

$ML64 = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
$LINK = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
$LIB = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\lib.exe"
$CompilerDir = "D:\RawrXD-Compilers"
$OutputDir = "$CompilerDir\bin"

Write-Host "`n==================================================================" -ForegroundColor Cyan
Write-Host "  RawrXD Production Assembly Toolchain Builder" -ForegroundColor Cyan
Write-Host "==================================================================" -ForegroundColor Cyan

# Ensure output directory exists
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

$BuildTargets = @(
    @{
        Name = "x64 Encoder (Pure Struct-Based)"
        Source = "x64_encoder_pure.asm"
        Object = "x64_encoder_pure.obj"
        Output = "x64_encoder_pure.lib"
        Type = "Library"
        Description = "Struct-based x64 instruction encoder with 16 functions"
    },
    @{
        Name = "x64 Encoder (Context-Based)"
        Source = "x64_encoder_corrected.asm"
        Object = "x64_encoder_corrected.obj"
        Output = "x64_encoder.lib"
        Type = "Library"
        Description = "Context-based stateful encoder for sequential emission"
    },
    @{
        Name = "RoslynBox Hot-Patch Engine"
        Source = "RoslynBox.asm"
        Object = "RoslynBox.obj"
        Output = "roslynbox.exe"
        Type = "Executable"
        Description = "Runtime code patching and reflection engine"
    },
    @{
        Name = "Reverse Assembler Loop"
        Source = "RawrXD_ReverseAssemblerLoop.asm"
        Object = "RawrXD_ReverseAssemblerLoop.obj"
        Output = "reverse_asm.lib"
        Type = "Library"
        Description = "Disassembler with continuous loop processing"
    },
    @{
        Name = "PE Generator/Encoder"
        Source = "pe_generator.asm"
        Object = "pe_generator.obj"
        Output = "pe_generator.exe"
        Type = "Executable"
        Description = "Generates valid PE32+ executables from scratch"
    }
)

$SuccessCount = 0
$FailureCount = 0

foreach ($target in $BuildTargets) {
    Write-Host "`n[*] Building: $($target.Name)" -ForegroundColor Yellow
    Write-Host "    Source: $($target.Source)" -ForegroundColor Gray
    Write-Host "    Description: $($target.Description)" -ForegroundColor Gray
    
    Set-Location $CompilerDir
    
    # Check if source exists
    if (!(Test-Path "$CompilerDir\$($target.Source)")) {
        Write-Host "    [!] Source file not found, skipping..." -ForegroundColor Red
        $FailureCount++
        continue
    }
    
    # Assemble
    Write-Host "    [~] Assembling..." -ForegroundColor Cyan
    $assembleArgs = @("/c", "/nologo", "/Zi", "/Zf", "$($target.Source)")
    $assembleResult = & $ML64 $assembleArgs 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "    [X] Assembly failed!" -ForegroundColor Red
        Write-Host $assembleResult -ForegroundColor DarkRed
        $FailureCount++
        continue
    }
    
    # Link/Lib
    if ($target.Type -eq "Library") {
        Write-Host "    [~] Creating library..." -ForegroundColor Cyan
        $libArgs = @("/NOLOGO", "/OUT:$OutputDir\$($target.Output)", "$CompilerDir\$($target.Object)")
        $libResult = & $LIB $libArgs 2>&1
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "    [X] Library creation failed!" -ForegroundColor Red
            Write-Host $libResult -ForegroundColor DarkRed
            $FailureCount++
            continue
        }
    }
    elseif ($target.Type -eq "Executable") {
        Write-Host "    [~] Linking executable..." -ForegroundColor Cyan
        $linkArgs = @("/NOLOGO", "/SUBSYSTEM:CONSOLE", "/OUT:$OutputDir\$($target.Output)", 
                      "$CompilerDir\$($target.Object)", "kernel32.lib", "user32.lib")
        $linkResult = & $LINK $linkArgs 2>&1
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "    [X] Linking failed!" -ForegroundColor Red
            Write-Host $linkResult -ForegroundColor DarkRed
            $FailureCount++
            continue
        }
    }
    
    # Verify output
    if (Test-Path "$OutputDir\$($target.Output)") {
        $size = [math]::Round((Get-Item "$OutputDir\$($target.Output)").Length / 1KB, 2)
        Write-Host "    [+] Success! Output: $($target.Output) ($size KB)" -ForegroundColor Green
        $SuccessCount++
    }
    else {
        Write-Host "    [X] Output file not found!" -ForegroundColor Red
        $FailureCount++
    }
}

Write-Host "`n==================================================================" -ForegroundColor Cyan
Write-Host "  Build Summary" -ForegroundColor Cyan
Write-Host "==================================================================" -ForegroundColor Cyan
Write-Host "  Successful: $SuccessCount" -ForegroundColor Green
Write-Host "  Failed:     $FailureCount" -ForegroundColor $(if ($FailureCount -eq 0) { "Green" } else { "Red" })
Write-Host "`n  Output Directory: $OutputDir" -ForegroundColor Cyan

if (Test-Path $OutputDir) {
    Write-Host "`n  Built Components:" -ForegroundColor Cyan
    Get-ChildItem "$OutputDir\*" -Include "*.exe", "*.lib" | ForEach-Object {
        $size = [math]::Round($_.Length / 1KB, 2)
        Write-Host "    - $($_.Name) ($size KB)" -ForegroundColor Gray
    }
}

Write-Host "`n==================================================================`n" -ForegroundColor Cyan
