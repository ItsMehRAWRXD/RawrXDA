# Decode VS Code History Files and Reconstruct Directory Structure

$historyPath = "D:\RawrXD\history"
$reconstructedPath = "D:\RawrXD\reconstructed"

# Create reconstructed directory if it doesn't exist
if (!(Test-Path $reconstructedPath)) {
    New-Item -ItemType Directory -Path $reconstructedPath -Force
}

# Get all files in history directory
$files = Get-ChildItem -Path $historyPath -File

Write-Host "Processing $($files.Count) history files..."

foreach ($file in $files) {
    $encodedName = $file.Name

    # Decode the filename
    # Pattern: drive__path_components_filename.ext
    # Replace double underscores with colons and single underscores with backslashes
    $decodedPath = $encodedName -replace '^([a-z])__', '$1:\' -replace '_', '\'

    # Extract just the relative path part (everything after the drive letter and colon)
    $colonIndex = $decodedPath.IndexOf(':')
    if ($colonIndex -ge 0) {
        $relativePath = $decodedPath.Substring($colonIndex + 1).TrimStart('\')
    } else {
        $relativePath = $decodedPath
    }

    # Create full destination path under reconstructed folder
    $destinationPath = Join-Path $reconstructedPath $relativePath

    # Create directory structure if it doesn't exist
    $destinationDir = Split-Path $destinationPath -Parent
    if (!(Test-Path $destinationDir)) {
        New-Item -ItemType Directory -Path $destinationDir -Force
    }

    # Copy the file to reconstructed location
    Copy-Item -Path $file.FullName -Destination $destinationPath -Force

    Write-Host "Reconstructed: $relativePath"
}

Write-Host "Reconstruction complete. Files organized in: $reconstructedPath"