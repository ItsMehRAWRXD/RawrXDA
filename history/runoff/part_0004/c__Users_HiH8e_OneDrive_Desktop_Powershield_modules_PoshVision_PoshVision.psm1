#Requires -Version 7.2
<#!
.SYNOPSIS
    Local screenshot-to-text helper using Phi-3-vision ONNX – CPU-only, no Python.
.DESCRIPTION
    Downloads ONNX Runtime (CPU) on first run, loads a Phi-3-vision ONNX session,
    converts a bitmap to a normalized tensor, and returns a crude decoded answer.
#>

# Download ONNX Runtime CPU (first run)
$OnnxPkg = "$PSScriptRoot\onnx\runtimes\win-x64\native\onnxruntime.dll"
if (-not (Test-Path $OnnxPkg)) {
    Write-Host "Downloading ONNX runtime (CPU)…" -ForegroundColor Cyan
    $tmp = New-TemporaryFile | Rename-Item -NewName { $_ -replace 'tmp$', 'zip' } -PassThru
    Invoke-WebRequest https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime/1.17.3 -OutFile $tmp
    Expand-Archive $tmp "$PSScriptRoot\onnx" -Force
    Remove-Item $tmp
}
Add-Type -Path $OnnxPkg

$SCRIPT:VisionModel   = "$PSScriptRoot\phi3v-384.onnx"
$SCRIPT:TokenizerFile = "$PSScriptRoot\phi3v-tokenizer.json"

function Convert-BitmapToTensor {
    param([System.Drawing.Bitmap]$Bmp)
    $side = 384
    $resized = New-Object System.Drawing.Bitmap($side, $side)
    $g = [System.Drawing.Graphics]::FromImage($resized)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawImage($Bmp, 0, 0, $side, $side)
    $g.Dispose()

    $tensor = New-Object 'System.Single[,,,]' 1, 3, $side, $side
    for ($y = 0; $y -lt $side; $y++) {
        for ($x = 0; $x -lt $side; $x++) {
            $c = $resized.GetPixel($x, $y)
            $tensor[0, 0, $y, $x] = ($c.R / 127.5) - 1.0
            $tensor[0, 1, $y, $x] = ($c.G / 127.5) - 1.0
            $tensor[0, 2, $y, $x] = ($c.B / 127.5) - 1.0
        }
    }
    $resized.Dispose()
    $tensor
}

function Get-TokenIds {
    param([string]$Text)
    $exe = "$PSScriptRoot\phi3v-tokenizer.exe"
    if (Test-Path $exe) {
        $json = & $exe encode -t $Text | ConvertFrom-Json
        return [int[]]$json.ids
    }
    # fallback crude word-piece
    $words = $Text -split '\s+'
    [int[]]($words | ForEach-Object { [math]::Abs($_.GetHashCode()) % 32000 })
}

function Get-VisionAnswer {
    [CmdletBinding()]
    param(
        [string]$Prompt = "Describe the image in one sentence.",
        [string]$ImagePath,
        [double]$Temperature = 0.3
    )
    if (-not (Test-Path $SCRIPT:VisionModel)) {
        throw "Place phi3v-384.onnx in $PSScriptRoot"
    }

    if (-not $ImagePath) {
        Add-Type -AssemblyName System.Drawing, System.Windows.Forms
        $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
        $bmp    = New-Object System.Drawing.Bitmap($screen.Width, $screen.Height)
        $g      = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($screen.Location, [System.Drawing.Point]::Empty, $bmp.Size)
        $g.Dispose()
    } else {
        $bmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path $ImagePath))
    }

    $tensor = Convert-BitmapToTensor $bmp
    $bmp.Dispose()

    $inName1  = 'pixel_values'
    $inName2  = 'input_ids'
    $outName  = 'logits'

    $shape = New-Object 'System.Int64[]' 4
    0..3 | ForEach-Object { $shape[$_] = $tensor.GetLength($_) }
    $pin = [System.Runtime.InteropServices.GCHandle]::Alloc($tensor, 'Pinned')
    try {
        $ov = New-Object Microsoft.ML.OnnxRuntime.DenseTensor[float](
            [System.Memory[float]]::Create($pin.AddrOfPinnedObject(), $tensor.Length),
            $shape)

        $tokIds = Get-TokenIds $Prompt
        $tokShape = New-Object 'System.Int64[]' (2)
        $tokShape[0] = 1; $tokShape[1] = $tokIds.Count
        $tokTensor = New-Object 'System.Int32[]' $tokIds.Count
        for ($i = 0; $i -lt $tokIds.Count; $i++) { $tokTensor[$i] = $tokIds[$i] }
        $ot = New-Object Microsoft.ML.OnnxRuntime.DenseTensor[int32](
            [System.ReadOnlyMemory[int32]]::Create($tokTensor),
            $tokShape)

        $session = New-Object Microsoft.ML.OnnxRuntime.InferenceSession($SCRIPT:VisionModel)
        $inputs = New-Object 'System.Collections.Generic.List[Microsoft.ML.OnnxRuntime.NamedOnnxValue]'
        $inputs.Add([Microsoft.ML.OnnxRuntime.NamedOnnxValue]::CreateFromTensor($inName1, $ov))
        $inputs.Add([Microsoft.ML.OnnxRuntime.NamedOnnxValue]::CreateFromTensor($inName2, $ot))

        $outputs = $session.Run($inputs)
        $tensorOut = $outputs[0].AsTensor[float]()
        $logits = $tensorOut.ToArray()

        $maxIdx = 0; $maxVal = $logits[0]
        for ($i = 1; $i -lt $logits.Count; $i++) { if ($logits[$i] -gt $maxVal) { $maxVal = $logits[$i]; $maxIdx = $i } }
        $answer = "Token-$maxIdx"
        if (Test-Path "$PSScriptRoot\phi3v-tokenizer.exe") { $answer = & "$PSScriptRoot\phi3v-tokenizer.exe" decode -i $maxIdx }
        [pscustomobject]@{ Prompt = $Prompt; Answer = $answer }
    } finally {
        $pin.Free()
    }
}

Export-ModuleMember -Function Get-VisionAnswer
