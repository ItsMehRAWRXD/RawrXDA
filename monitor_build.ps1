for ($i=1; $i -le 20; $i++) {
    Start-Sleep 30
    $cl = (Get-Process -Name cl -ErrorAction SilentlyContinue | Measure-Object).Count
    $nmake = (Get-Process -Name nmake -ErrorAction SilentlyContinue | Measure-Object).Count
    $sz = (Get-Item d:\rawrxd\build\build_output2.log -ErrorAction SilentlyContinue).Length
    $tail = (Get-Content d:\rawrxd\build\build_output2.log -Tail 1)
    Write-Host "[${i}] cl=$cl nmake=$nmake log=${sz}b last=$tail"
    if ($cl -eq 0 -and $nmake -eq 0) {
        Write-Host "BUILD DONE"
        Write-Host "---TAIL---"
        Get-Content d:\rawrxd\build\build_output2.log -Tail 30
        Write-Host "---ERRORS---"
        Select-String -Path d:\rawrxd\build\build_output2.log -Pattern "error C|error LNK|EXIT_CODE|fatal error" | Select-Object -Last 15
        break
    }
}
