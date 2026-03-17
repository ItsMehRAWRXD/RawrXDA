# The Nuclear Option: Comment out any line with remaining Qt keywords

$keywords = @("QObject", "qobject_cast", "QWebSocket", "QtCharts", "QChartView", "QT_BEGIN_NAMESPACE", "QT_END_NAMESPACE")
$pattern = ($keywords -join "|")

$files = Get-ChildItem -Path "D:\rawrxd\src" -Recurse -Include *.cpp,*.h,*.hpp

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName
    $newContent = @()
    $modified = $false
    
    foreach ($line in $content) {
        if ($line -match $pattern) {
            $newContent += "// REMOVED_QT: $line"
            $modified = $true
        } else {
            $newContent += $line
        }
    }
    
    if ($modified) {
        $newContent | Set-Content -Path $file.FullName
        Write-Host "Nuked Qt in $file"
    }
}
