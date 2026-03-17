param(
    [string]$GGUFFile = "D:\OllamaModels\BigDaddyG-Q2_K-PRUNED-16GB.gguf"
)

Write-Host "=== Raw GGUF File Analysis ===" -ForegroundColor Cyan
Write-Host "File: $GGUFFile`n"

$stream = [System.IO.File]::OpenRead($GGUFFile)
$reader = New-Object System.IO.BinaryReader($stream)

try {
    # Read header
    $magic = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
    $version = $reader.ReadUInt32()
    $tensorCount = $reader.ReadUInt64()
    $metaCount = $reader.ReadUInt64()
    
    Write-Host "Magic: $magic"
    Write-Host "Version: $version"
    Write-Host "Tensors: $tensorCount"
    Write-Host "Metadata: $metaCount`n"
    
    Write-Host "=== Metadata Keys ===" -ForegroundColor Yellow
    for ($i = 0; $i -lt [Math]::Min($metaCount, 25); $i++) {
        $keyLen = $reader.ReadUInt64()
        if ($keyLen -gt 1000) {
            Write-Host "[$i] Invalid key length: $keyLen (at pos $($stream.Position - 8))" -ForegroundColor Red
            break
        }
        
        $keyBytes = $reader.ReadBytes([int]$keyLen)
        $key = [System.Text.Encoding]::UTF8.GetString($keyBytes)
        $valueType = $reader.ReadUInt32()
        
        # Parse/skip value based on type
        $valueDesc = ""
        switch ($valueType) {
            0 { $reader.ReadByte(); $valueDesc = "Uint8" }
            1 { $reader.ReadSByte(); $valueDesc = "Int8" }
            2 { $reader.ReadUInt16(); $valueDesc = "Uint16" }
            3 { $reader.ReadInt16(); $valueDesc = "Int16" }
            4 { $val = $reader.ReadUInt32(); $valueDesc = "Uint32: $val" }
            5 { $reader.ReadInt32(); $valueDesc = "Int32" }
            6 { $reader.ReadSingle(); $valueDesc = "Float32" }
            7 { $reader.ReadByte(); $valueDesc = "Bool" }
            8 {  # String
                $strLen = $reader.ReadUInt64()
                if ($strLen -gt 100000) {
                    $valueDesc = "String (len: $strLen - TOO LARGE)"
                    break
                }
                $strBytes = $reader.ReadBytes([int]$strLen)
                $str = [System.Text.Encoding]::UTF8.GetString($strBytes)
                $valueDesc = "String: '$str'"
            }
            9 {  # Array
                $elemType = $reader.ReadUInt32()
                $arrayLen = $reader.ReadUInt64()
                $valueDesc = "Array[$arrayLen] type=$elemType"
                # Skip elements
                for ($j = 0; $j -lt $arrayLen; $j++) {
                    switch ($elemType) {
                        6 { $reader.ReadSingle() }  # Float32
                        default { break }
                    }
                }
            }
            10 { $reader.ReadUInt64(); $valueDesc = "Uint64" }
            11 { $reader.ReadInt64(); $valueDesc = "Int64" }
            12 { $reader.ReadDouble(); $valueDesc = "Float64" }
            default { $valueDesc = "Unknown($valueType)" }
        }
        
        Write-Host "[$i] $key = $valueDesc"
    }
    
    Write-Host "`n=== Tensor Info ===" -ForegroundColor Yellow
    Write-Host "Current file position: $($stream.Position)"
}
finally {
    $reader.Close()
    $stream.Close()
}
