# RawrXD One-Liner Generator - Clean Version
# Creates a compressed, encoded one-liner for on-the-fly autonomous agent execution
# Version: 3.0.2 - Production Ready

param(
    [Parameter(Mandatory=$false)]
    [string]$InputScript = "D:\lazy init ide\Execute-AutonomousAgent-Final.ps1",
    
    [Parameter(Mandatory=$false)]
    [string]$OutputFile = "D:\lazy init ide\OneLiner-AutonomousAgent-Clean.txt",
    
    [Parameter(Mandatory=$false)]
    [switch]$Execute = $false,
    
    [Parameter(Mandatory=$false)]
    [int]$MaxIterations = 5,
    
    [Parameter(Mandatory=$false)]
    [int]$SleepIntervalMs = 3000
)

Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                                                                   ║" -ForegroundColor Magenta
Write-Host "║         RawrXD ONE-LINER GENERATOR - CLEAN                      ║" -ForegroundColor Magenta
Write-Host "║                    Version 3.0.2 - Production Ready               ║" -ForegroundColor Magenta
Write-Host "║                                                                   ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""

# Read the input script
Write-Host "Reading input script: $InputScript" -ForegroundColor Cyan
try {
    $scriptContent = Get-Content -Path $InputScript -Raw -ErrorAction Stop
    $originalSize = $scriptContent.Length
    Write-Host "  ✓ Read $originalSize bytes" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Failed to read script: $_" -ForegroundColor Red
    exit 1
}

# Remove param block completely and add $args parsing
Write-Host "Removing param block and adding `$args parsing..." -ForegroundColor Cyan
try {
    # Remove the param block including all parameters
    $scriptContent = $scriptContent -replace '(?s)param\s*\([^)]+Mandatory=\$false[^)]+\)\s*\n\s*\[int\]\$MaxIterations[^\n]+\n\s*\[Parameter[^\)]+\)\s*\n\s*\[int\]\$SleepIntervalMs[^\n]+\n\s*\[Parameter[^\)]+\)\s*\n\s*\[switch\]\$WhatIf[^\n]+\n\s*\)\s*', ''
    
    # Add $args parsing at the very beginning
    $argsParsing = @"
# Parse arguments from `$args
`$MaxIterations = if (`$args.Count -gt 0) { [int]`$args[0] } else { 5 }
`$SleepIntervalMs = if (`$args.Count -gt 1) { [int]`$args[1] } else { 3000 }
`$WhatIf = if (`$args.Count -gt 2) { [bool]`$args[2] } else { `$false }

"@
    
    $scriptContent = $argsParsing + $scriptContent
    $modifiedSize = $scriptContent.Length
    Write-Host "  ✓ Modified to $modifiedSize bytes" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Failed to modify script: $_" -ForegroundColor Red
    exit 1
}

# Compress the script using Gzip
Write-Host "Compressing script..." -ForegroundColor Cyan
try {
    # Convert to bytes
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($scriptContent)
    
    # Create memory stream and compress
    $memoryStream = New-Object System.IO.MemoryStream
    $gzipStream = New-Object System.IO.Compression.GzipStream($memoryStream, [System.IO.Compression.CompressionMode]::Compress)
    
    # Write bytes to gzip stream
    $gzipStream.Write($bytes, 0, $bytes.Length)
    $gzipStream.Close()
    
    # Get compressed bytes
    $compressedBytes = $memoryStream.ToArray()
    $compressedSize = $compressedBytes.Length
    
    # Calculate compression ratio
    $compressionRatio = [math]::Round((1 - ($compressedSize / $modifiedSize)) * 100, 2)
    
    Write-Host "  ✓ Compressed to $compressedSize bytes" -ForegroundColor Green
    Write-Host "  ✓ Compression ratio: $compressionRatio%" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Failed to compress: $_" -ForegroundColor Red
    exit 1
}

# Base64 encode the compressed data
Write-Host "Base64 encoding compressed data..." -ForegroundColor Cyan
try {
    $base64String = [Convert]::ToBase64String($compressedBytes)
    $base64Size = $base64String.Length
    Write-Host "  ✓ Base64 string: $base64Size characters" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Failed to encode: $_" -ForegroundColor Red
    exit 1
}

# Create the one-liner
Write-Host "Creating one-liner..." -ForegroundColor Cyan

# Build the one-liner with parameters
$oneLiner = @"
powershell -NoProfile -ExecutionPolicy Bypass -Command "`$s='$base64String';`$g=[IO.Compression.GzipStream];`$m=New-Object IO.MemoryStream(,[Convert]::FromBase64String(`$s));`$d=New-Object `$g(`$m,[IO.Compression.CompressionMode]::Decompress);`$r=New-Object IO.StreamReader(`$d);`$c=`$r.ReadToEnd(); `$args=@($MaxIterations, $SleepIntervalMs); Invoke-Expression `$c"
"@

Write-Host "  ✓ One-liner created" -ForegroundColor Green
Write-Host ""
Write-Host "One-Liner Preview:" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host $oneLiner -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host ""

# Save to file
Write-Host "Saving one-liner to: $OutputFile" -ForegroundColor Cyan
try {
    $oneLiner | Out-File -FilePath $OutputFile -Encoding UTF8 -Force
    Write-Host "  ✓ Saved to file" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Failed to save: $_" -ForegroundColor Red
    exit 1
}

# Optionally execute the one-liner
if ($Execute) {
    Write-Host ""
    Write-Host "Executing one-liner..." -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
    
    # Extract and execute the command from the one-liner
    $command = $oneLiner.Trim()
    
    try {
        # Use Start-Process to avoid variable scope issues
        $processArgs = @{
            FilePath = "powershell.exe"
            ArgumentList = @(
                "-NoProfile",
                "-ExecutionPolicy", "Bypass",
                "-Command", "`$s='$base64String';`$g=[IO.Compression.GzipStream];`$m=New-Object IO.MemoryStream(,[Convert]::FromBase64String(`$s));`$d=New-Object `$g(`$m,[IO.Compression.CompressionMode]::Decompress);`$r=New-Object IO.StreamReader(`$d);`$c=`$r.ReadToEnd(); `$args=@($MaxIterations, $SleepIntervalMs); Invoke-Expression `$c"
            )
            Wait = $true
            NoNewWindow = $true
        }
        
        Start-Process @processArgs
        
        Write-Host ""
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host "✓ One-liner executed successfully" -ForegroundColor Green
    } catch {
        Write-Host ""
        Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
        Write-Host "✗ One-liner execution failed: $_" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║              ONE-LINER GENERATION COMPLETE                        ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""
Write-Host "Summary:" -ForegroundColor Yellow
Write-Host "  Original size: $originalSize bytes" -ForegroundColor White
Write-Host "  Modified size: $modifiedSize bytes" -ForegroundColor White
Write-Host "  Compressed size: $compressedSize bytes" -ForegroundColor White
Write-Host "  Base64 size: $base64Size characters" -ForegroundColor White
Write-Host "  Compression ratio: $compressionRatio%" -ForegroundColor White
Write-Host "  Output file: $OutputFile" -ForegroundColor White
Write-Host ""
Write-Host "Usage:" -ForegroundColor Yellow
Write-Host "  Copy the one-liner above and paste into any PowerShell terminal" -ForegroundColor White
Write-Host "  Or run: .\" -NoNewline -ForegroundColor White
Write-Host "Generate-OneLiner-Clean.ps1" -ForegroundColor Green -NoNewline
Write-Host " -Execute" -ForegroundColor White
Write-Host ""

exit 0