$ErrorActionPreference = 'Continue'
$historyDir = "C:\Users\HiH8e\AppData\Roaming\Code\User\History"
$outDir = "D:\RawrXD\history\all_versions"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

Write-Host "Scanning VS Code History for all versions..." -ForegroundColor Cyan
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
            
            if ($decodedPath -match "(?i)rawrxd\\(.*)") {
                $relPath = $matches[1]
                $realDestDir = Join-Path $outDir $relPath
                if (-not (Test-Path $realDestDir)) { New-Item -ItemType Directory -Path $realDestDir -Force | Out-Null }
                
                foreach ($entry in $content.entries) {
                    $sourceFile = Join-Path $entryFile.Directory.FullName $entry.id
                    if (Test-Path $sourceFile) {
                        $destFile = Join-Path $realDestDir "$($entry.timestamp)_$($entry.id)"
                        Copy-Item $sourceFile -Destination $destFile -Force
                        $recovered++
                    }
                }
            }
        }
    } catch { }
}

Write-Host "Processed $count history entries. Recovered $recovered RawrXD file versions to $outDir" -ForegroundColor Green
