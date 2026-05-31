# Comprehensive stub category scanner for RawrXD
$ErrorActionPreference = 'SilentlyContinue'
$results = @{}

# 1. Broken MASM files needing syntax fixes
$masmBroken = Get-ChildItem -Recurse -Include *.asm -Path src/asm, src/kernels | Where-Object {
    $content = Get-Content $_.FullName -Raw
    $content -match 'scaffold|placeholder|TODO.*fix.*syntax|FIXME.*ml64|__real@|\.model flat'
}
$results['Broken MASM'] = $masmBroken.Count

# 2. Headers with missing .cpp implementations
$headers = Get-ChildItem -Recurse -Include *.h,*.hpp -Path src,include | Where-Object {
    $base = $_.BaseName
    $cppPath = Join-Path $_.DirectoryName ($base + '.cpp')
    $implPath = Join-Path $_.DirectoryName ($base + '_impl.cpp')
    $hasDecl = (Get-Content $_.FullName -Raw) -match 'class\s+\w+|struct\s+\w+|void\s+\w+\s*\('
    $hasCpp = Test-Path $cppPath
    $hasImpl = Test-Path $implPath
    $hasDecl -and -not $hasCpp -and -not $hasImpl -and ($_.Name -notmatch '_stub|stub_|mock_|fake_')
}
$results['Headers missing cpp'] = $headers.Count

# 3. Linker stub/fallback files
$linkerStubs = Get-ChildItem -Recurse -Path src | Where-Object {
    $_.Name -match '_link_shims?\.cpp$|_fallback[^/]*\.cpp$|_stubs?\.cpp$|stub_.*\.cpp$|shim_.*\.cpp$|_(mock|fake)\.cpp$'
}
$results['Linker stubs'] = $linkerStubs.Count

# 4. Auto-generated stub handlers
$autoStubs = Get-ChildItem -Recurse -Path src | Where-Object {
    $c = Get-Content $_.FullName -Raw
    $_.Name -match 'missing_handler|auto_stub|generated_stub|placeholder_impl' -or
    ($c -match '//\s*AUTO-GENERATED|//\s*STUB|//\s*PLACEHOLDER' -and $c -match '\{\s*\}|return\s+0;|return\s+false;')
}
$results['Auto stubs'] = $autoStubs.Count

# 5. SSOT missing handlers
$ssotMissing = Get-ChildItem -Recurse -Path src | Where-Object {
    $c = Get-Content $_.FullName -Raw
    $c -match 'SSOT.*missing|missing.*SSOT|ssot_handler.*stub|ssot.*fallback'
}
$results['SSOT missing'] = $ssotMissing.Count

# 6. Security stubs
$secStubs = Get-ChildItem -Recurse -Path src/security | Where-Object {
    $c = Get-Content $_.FullName -Raw
    $c -match 'stub|placeholder|TODO|FIXME|mock|fake' -and $_.Extension -match 'cpp|h'
}
$results['Security stubs'] = $secStubs.Count

# 7. GPU backend stubs
$gpuStubs = Get-ChildItem -Recurse -Path src/gpu, src/core | Where-Object {
    $c = Get-Content $_.FullName -Raw
    ($_.Name -match 'gpu.*stub|vulkan.*stub|cuda.*stub|hip.*stub|dml.*stub' -or
     $c -match 'GPU.*stub|gpu.*fallback|vulkan.*placeholder') -and $_.Extension -match 'cpp|h'
}
$results['GPU stubs'] = $gpuStubs.Count

# 8. Extension system stubs
$extStubs = Get-ChildItem -Recurse -Path src/extensions, src/core/extension* | Where-Object {
    $c = Get-Content $_.FullName -Raw
    $c -match 'extension.*stub|ext.*placeholder|extension.*fallback|extension.*mock' -and $_.Extension -match 'cpp|h'
}
$results['Extension stubs'] = $extStubs.Count

# 9. Runtime stubs
$rtStubs = Get-ChildItem -Recurse -Path src/runtime* | Where-Object {
    $c = Get-Content $_.FullName -Raw
    $c -match 'stub|placeholder|TODO|FIXME|mock' -and $_.Extension -match 'cpp|h'
}
$results['Runtime stubs'] = $rtStubs.Count

# 10. JS extension host stubs
$jsStubs = Get-ChildItem -Recurse -Path src/core/js_extension* | Where-Object {
    $c = Get-Content $_.FullName -Raw
    $c -match 'stub|placeholder|TODO|FIXME|mock' -and $_.Extension -match 'cpp|h'
}
$results['JS stubs'] = $jsStubs.Count

# 11. Test stub files
$testStubs = Get-ChildItem -Recurse -Path tests, src/test* | Where-Object {
    $_.Name -match 'stub|mock|fake|placeholder' -and $_.Extension -match 'cpp|h'
}
$results['Test stubs'] = $testStubs.Count

# 12. Ship/archived stubs
$shipStubs = Get-ChildItem -Recurse -Path Ship | Where-Object {
    $c = Get-Content $_.FullName -Raw
    $c -match 'stub|placeholder|TODO|FIXME|mock|scaffold' -and $_.Extension -match 'cpp|h'
}
$results['Ship stubs'] = $shipStubs.Count

# 13. Resource file gaps
$resGaps = Get-ChildItem -Recurse -Path src/res | Where-Object {
    $_.Extension -match 'rc|res' -and (Get-Content $_.FullName -Raw) -match 'placeholder|TODO|stub|missing'
}
$results['Resource gaps'] = $resGaps.Count

Write-Host '=== RAWRXD STUB CATEGORY AUDIT ===' -ForegroundColor Cyan
$total = 0
foreach ($cat in $results.Keys | Sort-Object) {
    $count = $results[$cat]
    $total += $count
    $color = if ($count -gt 0) { 'Red' } else { 'Green' }
    Write-Host ('{0,-30} : {1,4}' -f $cat, $count) -ForegroundColor $color
}
Write-Host ('{0,-30} : {1,4}' -f 'TOTAL', $total) -ForegroundColor Yellow
Write-Host '=================================' -ForegroundColor Cyan

# Export detailed lists
$masmBroken | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_masm_broken.txt
$headers | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_headers_missing_cpp.txt
$linkerStubs | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_linker_stubs.txt
$autoStubs | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_auto_stubs.txt
$ssotMissing | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_ssot_missing.txt
$secStubs | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_security_stubs.txt
$gpuStubs | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_gpu_stubs.txt
$extStubs | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_extension_stubs.txt
$rtStubs | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_runtime_stubs.txt
$jsStubs | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_js_stubs.txt
$testStubs | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_test_stubs.txt
$shipStubs | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_ship_stubs.txt
$resGaps | Select-Object -ExpandProperty FullName | Out-File d:\rawrxd\audit_resource_gaps.txt
