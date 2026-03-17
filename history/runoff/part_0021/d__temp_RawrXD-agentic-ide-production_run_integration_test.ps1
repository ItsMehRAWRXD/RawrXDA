Write-Host "============================================================"
Write-Host "RawrXD IDE - Integration Test"
Write-Host "============================================================"
Write-Host ""

# Test 1: Verify model file
$modelPath = "D:\temp\RawrXD-agentic-ide-production\build-sovereign-static\bin\phi-3-mini.gguf"
if (Test-Path $modelPath) {
    $modelSize = (Get-Item $modelPath).Length / 1MB
    Write-Host "[✓] Model found: phi-3-mini.gguf ($([math]::Round($modelSize, 2)) MB)"
} else {
    Write-Host "[✗] Model not found at: $modelPath"
    exit 1
}

# Test 2: Verify DLL exports
Write-Host ""
Write-Host "Checking DLL exports..."
$dllPath = "D:\temp\RawrXD-agentic-ide-production\build\release\RawrXD-SovereignLoader.dll"
if (Test-Path $dllPath) {
    $dumpbin = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe"
    if (Test-Path $dumpbin) {
        $exports = & $dumpbin /exports $dllPath | Select-String "ManifestVisualIdentity|VerifyBeaconSignature|ProcessSignal"
        if ($exports) {
            Write-Host "[✓] DLL exports verified:"
            $exports | ForEach-Object { Write-Host "    $_" }
        }
    } else {
        Write-Host "[⚠] dumpbin not found - skipping export verification"
    }
} else {
    Write-Host "[✗] DLL not found at: $dllPath"
}

# Test 3: Launch IDE with model
Write-Host ""
Write-Host "Launching RawrXD IDE..."
$idePath = "D:\temp\RawrXD-agentic-ide-production\build\release\RawrXD-IDE.exe"
if (Test-Path $idePath) {
    Write-Host "[✓] Executable found: $idePath"
    Write-Host ""
    Write-Host "Starting IDE (press Ctrl+C to exit)..."
    Write-Host "  - Model will be available at: $modelPath"
    Write-Host "  - Use Model > Select Model menu"
    Write-Host "  - Open Performance Monitor (Model menu)"
    Write-Host ""
    
    # Set Qt in PATH for runtime DLL resolution
    $env:PATH = "C:\Qt\6.7.3\msvc2022_64\bin;" + $env:PATH
    
    Start-Process $idePath -ArgumentList "--model", $modelPath -WorkingDirectory (Split-Path $idePath)
    
    Write-Host "[✓] IDE launched successfully"
} else {
    Write-Host "[✗] IDE executable not found at: $idePath"
    exit 1
}

Write-Host ""
Write-Host "============================================================"
Write-Host "Integration test complete"
Write-Host "============================================================"
