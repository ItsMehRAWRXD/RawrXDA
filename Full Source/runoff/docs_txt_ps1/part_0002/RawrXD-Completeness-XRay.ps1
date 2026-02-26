param(
    $SrcRoot = "$PSScriptRoot",
    $StubPattern = 'stub|TODO|FIXME|notimpl|place.?hold',
    $RealPattern = 'implements|real|production|enterprise'
)

$src = Get-ChildItem $SrcRoot -Recurse -Include *.cpp,*.c,*.h,*.asm,*.hpp
$total = $src.Count
$real = 0
$stub = 0
$score = 0

$src | ForEach-Object {
    $txt = Get-Content $_ -Raw
    $hasStub = $txt -match $StubPattern
    $hasReal = $txt -match $RealPattern
    if ($hasReal -and -not $hasStub) {
        $real++
    }
    if ($hasStub) {
        $stub++
    }
}

if ($total -gt 0) {
    $score = [math]::Round(($real / $total) * 100, 1)
}

Write-Host "RAW RXD COMPLETENESS: $score %  (Real:$real  Stub:$stub  Total:$total)"
