# RawrXD TODO Resolver - Client Script
param(
    [string]$SourcePath = "D:\lazy init ide\src"
)

$pipeName = "RawrXD_PatternBridge_V2"

# Try to connect via pipe first to see if server is running
function Invoke-RemoteClassify {
    param($Path)
    try {
        $client = New-Object System.IO.Pipes.NamedPipeClientStream(".", $pipeName, [System.IO.Pipes.PipeDirection]::InOut)
        $client.Connect(500) # 500ms timeout
        $writer = New-Object System.IO.StreamWriter($client)
        $reader = New-Object System.IO.StreamReader($client)
        $writer.AutoFlush = $true
        
        $writer.WriteLine("CLASSIFY|$Path")
        $response = $reader.ReadLine()
        
        $client.Close()
        return $response | ConvertFrom-Json
    } catch {
        return $null
    }
}

# Fallback to local
$sigFile = "D:\lazy init ide\bin\RawrXD_PatternBridge_Signatures.ps1"
if (Test-Path $sigFile) {
    . $sigFile | Out-Null
}

$files = Get-ChildItem $SourcePath -Include *.ps1,*.cpp,*.h,*.asm -Recurse -File -ErrorAction SilentlyContinue

Write-Host "Scanning $($files.Count) files in $SourcePath..." -ForegroundColor Cyan

foreach ($file in $files) {
    $res = Invoke-RemoteClassify -Path $file.FullName
    
    if (-not $res) {
        # Fallback local
        if (Get-Command Invoke-RawrXDClassification -ErrorAction SilentlyContinue) {
             $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
             $localRes = Invoke-RawrXDClassification -CodeBuffer $bytes
             $res = @{ Type = $localRes.TypeName; Confidence = $localRes.Confidence }
        }
    }
    
    if ($res) {
        if ($res.Type -ne "Unknown" -and $res.Type -ne "NonPattern") {
             Write-Host "[$($res.Type)] $($file.Name)" -ForegroundColor Yellow
        }
    }
}
Write-Host "Scan complete." -ForegroundColor Green
