$ErrorActionPreference = 'Continue'
$historyDir = "C:\Users\HiH8e\AppData\Roaming\Code\User\History"
$outDir = "D:\RawrXD\history"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }
$reconstructDir = Join-Path $outDir "reconstructed"
if (-not (Test-Path $reconstructDir)) { New-Item -ItemType Directory -Path $reconstructDir -Force | Out-Null }

Write-Host "Scanning VS Code History at $historyDir..." -ForegroundColor Cyan
$entriesFiles = Get-ChildItem -Path $historyDir -Filter "entries.json" -Recurse -ErrorAction SilentlyContinue

$count = 0
$recovered = 0

foreach ($entryFile in $entriesFiles) {
    $count++
    try {
        $content = Get-Content $entryFile.FullName -Raw | ConvertFrom-Json
        $resource = $content.resource
        if ($resource -match "^file:///(.*)") {
            $decodedPath = [uri]::UnescapeDataString($matches[1])
            $decodedPath = $decodedPath -replace "/", "\"
            
            $latestEntry = $content.entries | Sort-Object timestamp -Descending | Select-Object -First 1
            if ($latestEntry) {
                $sourceFile = Join-Path $entryFile.Directory.FullName $latestEntry.id
                if (Test-Path $sourceFile) {
                    # Flat dump
                    $safeName = $decodedPath -replace "[:\\]", "_"
                    $destFile = Join-Path $outDir $safeName
                    Copy-Item $sourceFile -Destination $destFile -Force
                    
                    # Reconstructed tree for rawrxd
                    if ($decodedPath -match "(?i)rawrxd\\(.*)") {
                        $relPath = $matches[1]
                        $realDest = Join-Path $reconstructDir $relPath
                        $realDestDir = Split-Path $realDest -Parent
                        if (-not (Test-Path $realDestDir)) { New-Item -ItemType Directory -Path $realDestDir -Force | Out-Null }
                        Copy-Item $sourceFile -Destination $realDest -Force
                        $recovered++
                    }
                }
            }
        }
    } catch {
        Write-Warning "Failed to process $($entryFile.FullName): $($_.Exception.Message)"
    }
}

Write-Host "Processed $count history entries. Recovered $recovered RawrXD files to $reconstructDir" -ForegroundColor Green
