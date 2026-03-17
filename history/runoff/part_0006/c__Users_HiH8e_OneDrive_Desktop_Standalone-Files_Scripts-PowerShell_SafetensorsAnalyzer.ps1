# Safetensors Model Analyzer & Modifier
# Analyzes Safetensors/GGUF model files and identifies potential censorship layers
# Based on the assembly code structure for single-blob Safetensors

param(
    [Parameter(Mandatory=$false)]
    [string]$ModelPath,
    
    [Parameter(Mandatory=$false)]
    [switch]$Analyze,
    
    [Parameter(Mandatory=$false)]
    [switch]$ExtractMetadata,
    
    [Parameter(Mandatory=$false)]
    [switch]$FindCensorshipLayers,
    
    [Parameter(Mandatory=$false)]
    [string]$OutputReport = "model_analysis_report.json"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  SAFETENSORS/GGUF MODEL ANALYZER" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Safetensors file structure:
# [8 bytes: header_size as little-endian u64]
# [header_size bytes: JSON metadata]
# [remaining bytes: tensor data]

function Read-SafetensorsHeader {
    param([string]$Path)
    
    if (-not (Test-Path $Path)) {
        Write-Host "Error: File not found: $Path" -ForegroundColor Red
        return $null
    }
    
    $fileStream = [System.IO.File]::OpenRead($Path)
    $reader = New-Object System.IO.BinaryReader($fileStream)
    
    try {
        # Read 8-byte header size (little-endian u64)
        $headerSize = $reader.ReadUInt64()
        Write-Host "Header Size: $headerSize bytes" -ForegroundColor Yellow
        
        # Read JSON header
        $headerBytes = $reader.ReadBytes($headerSize)
        $headerJson = [System.Text.Encoding]::UTF8.GetString($headerBytes)
        
        $metadata = ConvertFrom-Json $headerJson
        
        # Calculate tensor data size
        $tensorDataSize = $fileStream.Length - 8 - $headerSize
        
        return @{
            HeaderSize = $headerSize
            Metadata = $metadata
            TensorDataSize = $tensorDataSize
            TotalFileSize = $fileStream.Length
            FilePath = $Path
        }
    }
    finally {
        $reader.Close()
        $fileStream.Close()
    }
}

function Analyze-SafetensorsStructure {
    param($HeaderInfo)
    
    Write-Host "`nFile Analysis:" -ForegroundColor Cyan
    Write-Host "  File: $($HeaderInfo.FilePath)" -ForegroundColor Gray
    Write-Host "  Total Size: $($HeaderInfo.TotalFileSize) bytes ($([math]::Round($HeaderInfo.TotalFileSize/1MB, 2)) MB)" -ForegroundColor Gray
    Write-Host "  Header Size: $($HeaderInfo.HeaderSize) bytes" -ForegroundColor Gray
    Write-Host "  Tensor Data: $($HeaderInfo.TensorDataSize) bytes ($([math]::Round($HeaderInfo.TensorDataSize/1MB, 2)) MB)" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "Tensors Found:" -ForegroundColor Cyan
    $tensorCount = 0
    
    foreach ($tensorName in $HeaderInfo.Metadata.PSObject.Properties.Name) {
        if ($tensorName -eq '__metadata__') { continue }
        
        $tensor = $HeaderInfo.Metadata.$tensorName
        $tensorCount++
        
        Write-Host "  [$tensorCount] $tensorName" -ForegroundColor Yellow
        Write-Host "      dtype: $($tensor.dtype)" -ForegroundColor Gray
        Write-Host "      shape: [$($tensor.shape -join ', ')]" -ForegroundColor Gray
        
        if ($tensor.data_offsets) {
            $start = $tensor.data_offsets[0]
            $end = $tensor.data_offsets[1]
            $size = $end - $start
            Write-Host "      offsets: [$start, $end] (Size: $size bytes)" -ForegroundColor Gray
        }
    }
    
    Write-Host "`nTotal Tensors: $tensorCount" -ForegroundColor Green
}

function Find-PotentialCensorshipLayers {
    param($HeaderInfo)
    
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  CENSORSHIP LAYER DETECTION" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    $suspiciousLayers = @()
    
    # Common patterns in censored models:
    # 1. Layers with names containing: 'safety', 'filter', 'moderation', 'classifier'
    # 2. Small output layers (often binary classifiers for harmful content)
    # 3. Attention masks or embedding modifications
    
    $censorshipKeywords = @(
        'safety', 'filter', 'moderation', 'classifier', 'harmful',
        'toxicity', 'nsfw', 'content_filter', 'guardrail', 'restrict'
    )
    
    foreach ($tensorName in $HeaderInfo.Metadata.PSObject.Properties.Name) {
        if ($tensorName -eq '__metadata__') { continue }
        
        $tensor = $HeaderInfo.Metadata.$tensorName
        $suspicionScore = 0
        $reasons = @()
        
        # Check for censorship-related keywords
        foreach ($keyword in $censorshipKeywords) {
            if ($tensorName -like "*$keyword*") {
                $suspicionScore += 50
                $reasons += "Contains keyword: '$keyword'"
            }
        }
        
        # Check for small binary classifier patterns (shape ending in [2] or [1])
        if ($tensor.shape -and $tensor.shape.Count -gt 0) {
            $lastDim = $tensor.shape[-1]
            if ($lastDim -eq 2 -and $tensor.shape.Count -eq 2) {
                $suspicionScore += 30
                $reasons += "Binary classifier pattern (output dim: 2)"
            }
            if ($lastDim -eq 1 -and $tensorName -like "*output*") {
                $suspicionScore += 25
                $reasons += "Single output classifier"
            }
        }
        
        # Check for attention modification layers
        if ($tensorName -like "*attn*mask*" -or $tensorName -like "*attention*filter*") {
            $suspicionScore += 40
            $reasons += "Attention modification layer"
        }
        
        # Check for embedding filters
        if ($tensorName -like "*embed*filter*" -or $tensorName -like "*token*filter*") {
            $suspicionScore += 45
            $reasons += "Embedding/token filter"
        }
        
        if ($suspicionScore -gt 0) {
            $suspiciousLayers += @{
                Name = $tensorName
                Score = $suspicionScore
                Reasons = $reasons
                Dtype = $tensor.dtype
                Shape = $tensor.shape
                DataOffsets = $tensor.data_offsets
            }
        }
    }
    
    if ($suspiciousLayers.Count -eq 0) {
        Write-Host "✓ No obvious censorship layers detected" -ForegroundColor Green
    }
    else {
        Write-Host "⚠ Found $($suspiciousLayers.Count) potentially suspicious layer(s):" -ForegroundColor Yellow
        Write-Host ""
        
        foreach ($layer in ($suspiciousLayers | Sort-Object -Property Score -Descending)) {
            $color = if ($layer.Score -ge 70) { "Red" } 
                    elseif ($layer.Score -ge 40) { "Yellow" } 
                    else { "Cyan" }
            
            Write-Host "  Layer: $($layer.Name)" -ForegroundColor $color
            Write-Host "    Suspicion Score: $($layer.Score)/100" -ForegroundColor $color
            Write-Host "    Shape: [$($layer.Shape -join ', ')]" -ForegroundColor Gray
            Write-Host "    Reasons:" -ForegroundColor Gray
            foreach ($reason in $layer.Reasons) {
                Write-Host "      - $reason" -ForegroundColor Gray
            }
            Write-Host ""
        }
    }
    
    return $suspiciousLayers
}

function Generate-UncensoringReport {
    param($HeaderInfo, $SuspiciousLayers)
    
    $report = @{
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        ModelFile = $HeaderInfo.FilePath
        FileSize = $HeaderInfo.TotalFileSize
        TotalTensors = ($HeaderInfo.Metadata.PSObject.Properties.Name | Where-Object { $_ -ne '__metadata__' }).Count
        SuspiciousLayerCount = $SuspiciousLayers.Count
        SuspiciousLayers = $SuspiciousLayers
        Recommendations = @()
    }
    
    # Generate recommendations
    foreach ($layer in $SuspiciousLayers) {
        if ($layer.Score -ge 70) {
            $report.Recommendations += @{
                Layer = $layer.Name
                Action = "REMOVE - High confidence censorship layer"
                Method = "Zero out tensor data or remove from model architecture"
                Priority = "HIGH"
            }
        }
        elseif ($layer.Score -ge 40) {
            $report.Recommendations += @{
                Layer = $layer.Name
                Action = "INVESTIGATE - Likely filtering layer"
                Method = "Analyze activation patterns, consider neutralizing"
                Priority = "MEDIUM"
            }
        }
        else {
            $report.Recommendations += @{
                Layer = $layer.Name
                Action = "MONITOR - Possible safety component"
                Method = "Test model behavior with/without this layer"
                Priority = "LOW"
            }
        }
    }
    
    return $report
}

function Show-UncensoringInstructions {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  UNCENSORING INSTRUCTIONS" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "Methods to uncensor identified layers:" -ForegroundColor Yellow
    Write-Host ""
    
    Write-Host "1. TENSOR ZEROING (Safest)" -ForegroundColor Green
    Write-Host "   - Zero out the tensor data at specified offsets" -ForegroundColor Gray
    Write-Host "   - Preserves model structure but disables layer" -ForegroundColor Gray
    Write-Host "   - Reversible (keep backup)" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "2. LAYER REMOVAL (Advanced)" -ForegroundColor Yellow
    Write-Host "   - Rebuild Safetensors without suspicious layers" -ForegroundColor Gray
    Write-Host "   - Requires architecture knowledge" -ForegroundColor Gray
    Write-Host "   - May require model fine-tuning" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "3. WEIGHT MODIFICATION (Experimental)" -ForegroundColor Cyan
    Write-Host "   - Invert classifier weights (flip decisions)" -ForegroundColor Gray
    Write-Host "   - Replace with identity/passthrough matrices" -ForegroundColor Gray
    Write-Host "   - Requires understanding of layer purpose" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "4. ARCHITECTURE SURGERY (Expert)" -ForegroundColor Red
    Write-Host "   - Modify model config to skip layers" -ForegroundColor Gray
    Write-Host "   - Requires framework-specific knowledge" -ForegroundColor Gray
    Write-Host "   - May need recompilation/conversion" -ForegroundColor Gray
    Write-Host ""
}

# Main Execution
if (-not $ModelPath) {
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  .\SafetensorsAnalyzer.ps1 -ModelPath <path> -Analyze" -ForegroundColor Gray
    Write-Host "  .\SafetensorsAnalyzer.ps1 -ModelPath <path> -FindCensorshipLayers" -ForegroundColor Gray
    Write-Host "  .\SafetensorsAnalyzer.ps1 -ModelPath <path> -ExtractMetadata" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Example:" -ForegroundColor Yellow
    Write-Host "  .\SafetensorsAnalyzer.ps1 -ModelPath model.safetensors -FindCensorshipLayers" -ForegroundColor Gray
    exit
}

# Read the model file
$headerInfo = Read-SafetensorsHeader -Path $ModelPath

if (-not $headerInfo) {
    Write-Host "Failed to read model file" -ForegroundColor Red
    exit 1
}

# Perform requested operations
if ($Analyze -or $FindCensorshipLayers) {
    Analyze-SafetensorsStructure -HeaderInfo $headerInfo
}

if ($ExtractMetadata) {
    Write-Host "`nMetadata:" -ForegroundColor Cyan
    $headerInfo.Metadata | ConvertTo-Json -Depth 10 | Write-Host
}

$suspiciousLayers = @()
if ($FindCensorshipLayers) {
    $suspiciousLayers = Find-PotentialCensorshipLayers -HeaderInfo $headerInfo
    Show-UncensoringInstructions
}

# Generate report
if ($FindCensorshipLayers -and $suspiciousLayers.Count -gt 0) {
    $report = Generate-UncensoringReport -HeaderInfo $headerInfo -SuspiciousLayers $suspiciousLayers
    $reportJson = $report | ConvertTo-Json -Depth 10
    $reportJson | Out-File -FilePath $OutputReport -Encoding UTF8
    
    Write-Host "`nReport saved to: $OutputReport" -ForegroundColor Green
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Analysis Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
