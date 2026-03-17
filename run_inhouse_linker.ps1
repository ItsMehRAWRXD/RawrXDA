$ErrorActionPreference = "Stop"
$ExcludeDirs = @(".git", "dist", "node_modules", "temp", "source_audit", "crash_dumps", "build_fresh", "build_gold", "build_prod", "build_universal", "build_ide", "build_qt_free", "build_win32_gui_test", "CMakeFiles", "build_clean", "build_ide_ninja", "build_new", "build_test_parse")
$objFiles = Get-ChildItem -Path "D:\rawrxd" -Filter "*.obj" -Recurse -ErrorAction SilentlyContinue | Where-Object {
    $path = $_.FullName
    $name = $_.Name
    $exclude = $false
    if($name -match "\.cpp\.obj$" -or $name -match "\.c\.obj$" -or $name -match "\.cc\.obj$") { return $false }
    if($name -match "^bench_" -or $name -match "^test_" -or $name -match "compiler_from_scratch" -or $name -match "omega_pro" -or $name -match "OmegaPolyglot_v5") { return $false }
    if($name -match "dumpbin_final\.obj") { return $false }
    if($name -match "RawrXD_P2PRelay\.obj" -or $name -match "rawrxd-scc-nasm64\.obj" -or $name -match "RawrXD_CLI_Titan\.obj" -or $name -match "proof\.obj" -or $name -match "native_speed_kernels\.obj") { return $false }
    if($name -eq "mmap_loader.obj" -or $name -eq "lsp_jsonrpc.obj" -or $name -eq "kv_cache_mgr.obj" -or $name -eq "dequant_simd.obj" -or $name -eq "NUL.obj" -or $name -eq "temp_test.obj") { return $false }
    if($name -eq "agentic_AgentOllamaClient.obj" -or $name -eq "config_IDEConfig.obj" -or $name -eq "mmap_loader_stub.obj") { return $false }
    if($name -match "^CMakeC") { return $false }
    foreach($dir in $ExcludeDirs) { if($path -match "\\$dir\\" -or $path -match "CMakeFiles") { $exclude = $true; break } }
    return -not $exclude
}
$objFiles = $objFiles | Where-Object {
    try {
        $bytes = [System.IO.File]::ReadAllBytes($_.FullName)
        if ($bytes.Length -ge 4 -and $bytes[0] -eq 0 -and $bytes[1] -eq 0 -and $bytes[2] -eq 0xFF -and $bytes[3] -eq 0xFF) { return $false }
        return $true
    } catch { return $false }
}
$uniqueObjs = $objFiles | Sort-Object LastWriteTime -Descending | Group-Object Name | ForEach-Object { $_.Group[0] }
Write-Host "Linking $($uniqueObjs.Count) objects..."
D:\rawrxd\tools\inhouse\link_inhouse.ps1 -Inputs $uniqueObjs.FullName
