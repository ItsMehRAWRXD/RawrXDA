$todos = Import-Csv "all_todos.csv"
$filtered = $todos | Where-Object { 
    $_.File -match '\.ps1$' -and 
    $_.File -notmatch 'RawrXD.ps1' -and
    $_.File -notmatch 'test_todos.ps1' -and
    $_.File -notmatch 'dump_todos.ps1' -and
    $_.File -notmatch 'analyze_todos.ps1' -and
    $_.Text -match "TODO"
}
$output = "Actionable .ps1 TODOs: $($filtered.Count)`n"
$output += ($filtered | Format-Table -AutoSize | Out-String)
Set-Content -Path "ps1_todos.txt" -Value $output