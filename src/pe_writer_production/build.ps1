# PE Writer Production Build Script for ASM
# Supports Windows x64 MASM64 builds

Write-Host "=== RAWRXD PE32 Emitter Monolithic Build Script ==="

# Check for MASM64 (ml64.exe)
$ml64Path = Get-Command ml64 -ErrorAction SilentlyContinue
if (-not $ml64Path) {
    # Try common paths
    $possiblePaths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\*\VC\bin\amd64\ml64.exe",
        "C:\masm64\bin\ml64.exe"
    )
    foreach ($path in $possiblePaths) {
        $ml64Path = Get-ChildItem $path -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($ml64Path) { break }
    }
    if (-not $ml64Path) {
        Write-Host "ERROR: MASM64 (ml64.exe) not found. Please install Visual Studio with C++ build tools or MASM64."
        exit 1
    }
}

# Set paths
$asmFile = "..\asm\RAWRXD_PE32_EMITTER_MONOLITHIC.asm"
$objFile = "RAWRXD_PE32_EMITTER_MONOLITHIC.obj"
$exeFile = "RAWRXD_PE32_EMITTER_MONOLITHIC.exe"

# Assemble
Write-Host "Assembling $asmFile..."
& $ml64Path /c /Fo"$objFile" "$asmFile"
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Assembly failed."
    exit 1
}

# Link
Write-Host "Linking $objFile..."
link "$objFile" /SUBSYSTEM:CONSOLE /ENTRY:main kernel32.lib /OUT:"$exeFile"
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Linking failed."
    exit 1
}

# Test run (optional)
Write-Host "Testing executable..."
& ".\$exeFile"
if ($LASTEXITCODE -ne 0) {
    Write-Host "WARNING: Executable test failed with code $LASTEXITCODE."
} else {
    Write-Host "SUCCESS: Executable ran successfully."
}

Write-Host "Build complete. Output: $exeFile"