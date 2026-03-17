# Model Blob Compression and Segmentation Script (PowerShell)
# Reduces storage footprint by compressing model files into chunks

param(
  [Parameter(Mandatory = $true)]
  [string]$ModelFile,
    
  [Parameter(Mandatory = $true)]
  [string]$OutputDir,
    
  [Parameter(Mandatory = $false)]
  [string]$ChunkSize = "1GB"
)

function Convert-ToBytes {
  param([string]$Size)
    
  $units = @{
    'KB' = 1024
    'MB' = 1024 * 1024
    'GB' = 1024 * 1024 * 1024
    'TB' = 1024 * 1024 * 1024 * 1024
  }
    
  $number = [regex]::Match($Size, '\d+').Value
  $unit = [regex]::Match($Size, '[A-Z]+').Value
    
  return [long]$number * $units[$unit]
}

function Split-File {
  param(
    [string]$InputFile,
    [string]$OutputDir,
    [long]$ChunkSizeBytes
  )
    
  $buffer = New-Object byte[] $ChunkSizeBytes
  $inputStream = [System.IO.File]::OpenRead($InputFile)
  $chunkIndex = 0
    
  try {
    while ($inputStream.Position -lt $inputStream.Length) {
      $chunkFile = Join-Path $OutputDir "chunk_$("{0:D3}" -f $chunkIndex)"
      $outputStream = [System.IO.File]::Create($chunkFile)
            
      try {
        $bytesRead = $inputStream.Read($buffer, 0, $ChunkSizeBytes)
        $outputStream.Write($buffer, 0, $bytesRead)
        Write-Host "Created chunk: $(Split-Path $chunkFile -Leaf) ($bytesRead bytes)"
      }
      finally {
        $outputStream.Close()
      }
            
      $chunkIndex++
    }
  }
  finally {
    $inputStream.Close()
  }
    
  return $chunkIndex
}

# Validate inputs
if (!(Test-Path $ModelFile)) {
  Write-Error "Model file not found: $ModelFile"
  exit 1
}

Write-Host "Compressing model: $ModelFile"
Write-Host "Output directory: $OutputDir"
Write-Host "Chunk size: $ChunkSize"

# Create output directory
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

# Get model info
$modelInfo = Get-Item $ModelFile
$originalSize = $modelInfo.Length
Write-Host "Original model size: $([math]::Round($originalSize / 1GB, 2)) GB"

# Convert chunk size to bytes
$chunkSizeBytes = Convert-ToBytes $ChunkSize

# Split model into chunks
Write-Host "Splitting model into chunks..."
$chunkCount = Split-File -InputFile $ModelFile -OutputDir $OutputDir -ChunkSizeBytes $chunkSizeBytes

# Compress each chunk
Write-Host "Compressing chunks..."
$compressedSize = 0
$chunks = @()

for ($i = 0; $i -lt $chunkCount; $i++) {
  $chunkFile = Join-Path $OutputDir "chunk_$("{0:D3}" -f $i)"
  $compressedFile = "$chunkFile.gz"
    
  Write-Host "Compressing chunk_$("{0:D3}" -f $i)..."
    
  # Use 7-Zip if available, otherwise use .NET compression
  if (Get-Command 7z -ErrorAction SilentlyContinue) {
    & 7z a -tgzip -mx9 $compressedFile $chunkFile | Out-Null
  }
  else {
    # .NET compression fallback
    $inputBytes = [System.IO.File]::ReadAllBytes($chunkFile)
    $outputStream = New-Object System.IO.FileStream($compressedFile, [System.IO.FileMode]::Create)
    $gzipStream = New-Object System.IO.Compression.GZipStream($outputStream, [System.IO.Compression.CompressionMode]::Compress)
    $gzipStream.Write($inputBytes, 0, $inputBytes.Length)
    $gzipStream.Close()
    $outputStream.Close()
  }
    
  # Remove original chunk
  Remove-Item $chunkFile
    
  $compressedInfo = Get-Item $compressedFile
  $compressedSize += $compressedInfo.Length
    
  $chunks += @{
    file            = $compressedInfo.Name
    compressed_size = $compressedInfo.Length
  }
}

# Create metadata file
$metadata = @{
  original_file = $modelInfo.Name
  original_size = $originalSize
  chunk_size    = $ChunkSize
  chunks        = $chunks
}

$metadataJson = $metadata | ConvertTo-Json -Depth 3
$metadataFile = Join-Path $OutputDir "model_metadata.json"
$metadataJson | Out-File -FilePath $metadataFile -Encoding UTF8

# Calculate compression ratio
$ratio = [math]::Round(($compressedSize * 100) / $originalSize, 2)
$spaceSaved = $originalSize - $compressedSize

Write-Host ""
Write-Host "Compression complete!" -ForegroundColor Green
Write-Host "Original size: $([math]::Round($originalSize / 1GB, 2)) GB"
Write-Host "Compressed size: $([math]::Round($compressedSize / 1GB, 2)) GB"
Write-Host "Compression ratio: $ratio%"
Write-Host "Space saved: $([math]::Round($spaceSaved / 1GB, 2)) GB"