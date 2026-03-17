# Install BigDaddyG Extension to Cursor
param(
    [string]$ExtensionSource = "C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\ide-extensions\bigdaddyg-copilot",
    [string]$CursorExtPath = "$env:APPDATA\Cursor\extensions"
)

Write-Host "Installing BigDaddyG Extension..." -ForegroundColor Cyan
Write-Host "Source: $ExtensionSource" -ForegroundColor Gray
Write-Host "Destination: $CursorExtPath" -ForegroundColor Gray

# Verify source exists
if (-not (Test-Path $ExtensionSource)) {
    Write-Host "ERROR: Source extension not found!" -ForegroundColor Red
    exit 1
}

# Create destination if needed
if (-not (Test-Path $CursorExtPath)) {
    Write-Host "Creating extensions directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $CursorExtPath -Force | Out-Null
}

# Remove old version if exists
$oldExt = "$CursorExtPath\bigdaddyg-copilot-1.0.0"
if (Test-Path $oldExt) {
    Write-Host "Removing old extension..." -ForegroundColor Yellow
    Remove-Item $oldExt -Recurse -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

# Copy new extension
try {
    Write-Host "Copying extension files..." -ForegroundColor Yellow
    Copy-Item -Path $ExtensionSource -Destination $oldExt -Recurse -Force
    
    if (Test-Path "$oldExt\out\extension.js") {
        Write-Host "✓ Extension successfully installed!" -ForegroundColor Green
        Write-Host "  Location: $oldExt" -ForegroundColor Green
        Write-Host "  Compiled file: out/extension.js" -ForegroundColor Green
        Write-Host "" -ForegroundColor Gray
        Write-Host "IMPORTANT: Close and restart Cursor to activate the extension!" -ForegroundColor Cyan
    } else {
        Write-Host "ERROR: Extension files incomplete!" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "ERROR: Failed to copy extension - $_" -ForegroundColor Red
    exit 1
}
