# PowerShell script for model compression on Windows
# Equivalent to compress_model.sh
# Usage: .\compress_model_windows.ps1 <model_file> <output_dir> [chunk_size]

param(
  [Parameter(Mandatory = $true)]
  [string]$ModelFile,
    
  [Parameter(Mandatory = $true)]
  [string]$OutputDir,
    
  [string]$ChunkSize = "1GB"
)

# Check if model file exists
if (-not (Test-Path $ModelFile)) {
  Write-Error "Model file not found: $ModelFile"
  exit 1
}

# Create output directory
if (-not (Test-Path $OutputDir)) {
  New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

Write-Host "Compressing model: $ModelFile"
Write-Host "Output directory: $OutputDir"
Write-Host "Chunk size: $ChunkSize"

# Get model size
$modelSize = (Get-Item $ModelFile).Length
Write-Host "Original model size: $(Format-FileSize $modelSize)"

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

# Parse chunk size
$chunkSizeBytes = switch -Regex ($ChunkSize) {
  '^(\d+)GB$' { [long]$matches[1] * 1GB }
  '^(\d+)MB$' { [long]$matches[1] * 1MB }
  '^(\d+)KB$' { [long]$matches[1] * 1KB }
  default { [long]$ChunkSize }
}

# Split file into chunks
Write-Host "Splitting model into chunks..."
$buffer = New-Object byte[] $chunkSizeBytes
$reader = [System.IO.File]::OpenRead($ModelFile)
$chunkIndex = 0

try {
  while ($true) {
    $bytesRead = $reader.Read($buffer, 0, $buffer.Length)
    if ($bytesRead -eq 0) { break }
        
    $chunkFile = Join-Path $OutputDir ("chunk_{0:D3}" -f $chunkIndex)
    $writer = [System.IO.File]::OpenWrite($chunkFile)
    $writer.Write($buffer, 0, $bytesRead)
    $writer.Close()
        
    $chunkIndex++
  }
}
finally {
  $reader.Close()
}

Write-Host "Created $chunkIndex chunks"

# Compress chunks using gzip (requires gzip.exe or 7zip)
Write-Host "Compressing chunks..."
$gzipAvailable = Get-Command gzip -ErrorAction SilentlyContinue
$sevenZipAvailable = Get-Command 7z -ErrorAction SilentlyContinue

if ($gzipAvailable) {
  foreach ($chunk in Get-ChildItem $OutputDir -Filter "chunk_*") {
    Write-Host "Compressing $($chunk.Name)..."
    & gzip -9 $chunk.FullName
  }
}
elseif ($sevenZipAvailable) {
  foreach ($chunk in Get-ChildItem $OutputDir -Filter "chunk_*") {
    Write-Host "Compressing $($chunk.Name)..."
    & 7z a -tgzip -mx9 "$($chunk.FullName).gz" $chunk.FullName
    Remove-Item $chunk.FullName
  }
}
else {
  Write-Warning "Neither gzip nor 7z found. Chunks will remain uncompressed."
  Write-Host "Please install gzip or 7-Zip for compression."
}

# Create metadata file
$metadata = @{
  original_file = Split-Path $ModelFile -Leaf
  original_size = $modelSize
  chunk_size    = $ChunkSize
  chunks        = @()
}

$compressedChunks = Get-ChildItem $OutputDir -Filter "chunk_*.gz"
if (-not $compressedChunks) {
  $compressedChunks = Get-ChildItem $OutputDir -Filter "chunk_*"
}

foreach ($chunk in $compressedChunks) {
  $metadata.chunks += @{
    file            = $chunk.Name
    compressed_size = $chunk.Length
  }
}

$metadata | ConvertTo-Json | Out-File (Join-Path $OutputDir "model_metadata.json") -Encoding UTF8

# Calculate compression stats
$compressedSize = ($metadata.chunks | Measure-Object -Property compressed_size -Sum).Sum
$ratio = if ($modelSize -gt 0) { [math]::Round(($compressedSize / $modelSize) * 100, 2) } else { 0 }
$saved = $modelSize - $compressedSize

Write-Host "Compression complete!"
Write-Host "Original size: $(Format-FileSize $modelSize)"
Write-Host "Compressed size: $(Format-FileSize $compressedSize)"
Write-Host "Compression ratio: $ratio%"
Write-Host "Space saved: $(Format-FileSize $saved)"