# PowerShell script for model decompression on Windows
# Equivalent to decompress_model.sh
# Usage: .\decompress_model_windows.ps1 <compressed_dir> <output_file> [temp_dir]

param(
    [Parameter(Mandatory=$true)]
    [string]$CompressedDir,
    
    [Parameter(Mandatory=$true)]
    [string]$OutputFile,
    
    [string]$TempDir = "$env:TEMP\model_temp"
)

# Check if compressed directory exists
if (-not (Test-Path $CompressedDir)) {
    Write-Error "Compressed directory not found: $CompressedDir"
    exit 1
}

$metadataFile = Join-Path $CompressedDir "model_metadata.json"
if (-not (Test-Path $metadataFile)) {
    Write-Error "Metadata file not found: $metadataFile"
    exit 1
}

Write-Host "Decompressing model from: $CompressedDir"
Write-Host "Output file: $OutputFile"

# Read metadata
$metadata = Get-Content $metadataFile | ConvertFrom-Json

$originalFile = $metadata.original_file
$originalSize = $metadata.original_size

Write-Host "Reconstructing: $originalFile"
Write-Host "Expected size: $(Format-FileSize $originalSize)"

# Function to format file size
function Format-FileSize {
    param([long]$size)
    $units = @("B", "KB", "MB", "GB", "TB")
    $unitIndex = 0
    $sizeDouble = $size
    while ($sizeDouble -ge 1024 -and $unitIndex -lt $units.Length - 1) {
        $sizeDouble /= 1024
        $unitIndex++
    }
    return "{0:N2} {1}" -f $sizeDouble, $units[$unitIndex]
}

# Create temp directory
if (-not (Test-Path $TempDir)) {
    New-Item -ItemType Directory -Path $TempDir | Out-Null
}

# Decompress chunks in order
Write-Host "Decompressing chunks..."
$reconstructedFile = Join-Path $TempDir "reconstructed_model"

# Check for decompression tools
$gzipAvailable = Get-Command gzip -ErrorAction SilentlyContinue
$sevenZipAvailable = Get-Command 7z -ErrorAction SilentlyContinue

foreach ($chunk in $metadata.chunks) {
    $chunkFile = Join-Path $CompressedDir $chunk.file
    if (-not (Test-Path $chunkFile)) {
        Write-Error "Chunk file not found: $chunkFile"
        exit 1
    }
    
    Write-Host "Processing $($chunk.file)..."
    
    if ($gzipAvailable) {
        & gzip -d -c $chunkFile | Out-File -FilePath $reconstructedFile -Append -Encoding Byte
    } elseif ($sevenZipAvailable) {
        $tempDecompressed = Join-Path $TempDir "temp_chunk"
        & 7z e -so $chunkFile | Out-File -FilePath $tempDecompressed -Encoding Byte
        Get-Content $tempDecompressed -Encoding Byte | Out-File -FilePath $reconstructedFile -Append -Encoding Byte
        Remove-Item $tempDecompressed
    } else {
        Write-Error "Neither gzip nor 7z found. Cannot decompress."
        exit 1
    }
}

# Verify size
$reconstructedSize = (Get-Item $reconstructedFile).Length
if ($reconstructedSize -ne $originalSize) {
    Write-Error "Size mismatch! Expected: $originalSize, Got: $reconstructedSize"
    exit 1
}

# Move to final location
Move-Item $reconstructedFile $OutputFile

# Cleanup
Remove-Item $TempDir -Recurse -Force

Write-Host "Decompression complete!"
Write-Host "Model reconstructed: $OutputFile"