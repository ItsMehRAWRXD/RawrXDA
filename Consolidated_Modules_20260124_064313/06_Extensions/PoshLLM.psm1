# ============================================
# PoshLLM Extension Module for RawrXD
# ============================================
# Category: AI/ML
# Purpose: From-scratch PowerShell GPT-style language model with BPE tokenization
# Author: RawrXD
# Version: 1.0.0
# ============================================

# Extension Metadata
$global:PoshLLMExtension = @{
    Id           = "poshllm"
    Name         = "PoshLLM"
    Description  = "From-scratch PowerShell GPT-style language model with BPE tokenization and transformer architecture"
    Author       = "RawrXD"
    Version      = "1.0.0"
    Language     = 0  # LANG_CUSTOM
    Capabilities = @("AI", "TextGeneration", "LanguageModel", "GPT", "Transformer")
    EditorType   = $null
    Dependencies = @()
    Enabled      = $true
}

<#
PoshLLM.psm1 – Minimal GPT-style decoder trained purely in PowerShell
No external libraries, no Python, no CUDA – just .NET and love.
Author: RawrXD
#>

# region --------------------------- math helpers ---------------------------
function Get-Gelu {
    param([double]$x)
    $x3 = $x * $x * $x
    0.5 * $x * (1.0 + [math]::Tanh(0.7978845608 * ($x + 0.044715 * $x3)))
}

function Get-Softmax {
    param([double[]]$x, [double]$Temp = 1.0)
    $max = ($x | Measure-Object -Maximum).Maximum
    $exp = $x | ForEach-Object { [math]::Exp(($_ - $max) / $Temp) }
    $sum = ($exp | Measure-Object -Sum).Sum
    $exp | ForEach-Object { $_ / $sum }
}

function Get-LayerNorm {
    param([double[]]$x, [double]$Eps = 1e-5)
    $mean = ($x | Measure-Object -Average).Average
    $var = ($x | ForEach-Object { ($_ - $mean) * ($_ - $mean) } | Measure-Object -Average).Average
    $x | ForEach-Object { ($_ - $mean) / [math]::Sqrt($var + $Eps) }
}
# endregion

# region ------------------------ tokenizer (BPE-ish) -----------------------
class SimpleTokenizer {
    static [System.Collections.Generic.List[string]] $Vocab
    static [System.Collections.Generic.Dictionary[string, int]] $Str2Id
    static [System.Collections.Generic.Dictionary[int, string]] $Id2Str
    static [int] $VocabSize

    static [void] Build([string[]]$Corpus, [int]$Merges) {
        if ($Merges -le 0) { $Merges = 300 }
        $all = [System.Collections.Generic.List[string]]::new()
        foreach ($line in $Corpus) {
            $line = $line.ToLower().Trim()
            if ($line -eq '') { continue }
            $chars = [char[]]$line
            $all.AddRange($chars | ForEach-Object { $_.ToString() })
            $all.Add('<|endoftext|>')
        }

        $pairs = @{}
        for ($i = 0; $i -lt $all.Count - 1; $i++) {
            $pair = $all[$i] + '‖' + $all[$i + 1]
            $pairs[$pair] = $pairs[$pair] + 1
        }

        $merged = [System.Collections.Generic.List[string]]::new($all)
        for ($merge = 0; $merge -lt $Merges; $merge++) {
            $best = ($pairs.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1).Name
            if (-not $best) { break }
            $a, $b = $best -split '‖'
            $i = 0
            while ($i -lt $merged.Count - 1) {
                if ($merged[$i] -ceq $a -and $merged[$i + 1] -ceq $b) {
                    $merged[$i] = $a + $b
                    $merged.RemoveAt($i + 1)
                }
                else { $i++ }
            }
            $pairs.Clear()
            for ($i = 0; $i -lt $merged.Count - 1; $i++) {
                $pair = $merged[$i] + '‖' + $merged[$i + 1]
                $pairs[$pair] = $pairs[$pair] + 1
            }
        }

        [SimpleTokenizer]::Vocab = [System.Collections.Generic.HashSet[string]]::new([string[]]$merged)
        [SimpleTokenizer]::Vocab.Add('<|unk|>')
        [SimpleTokenizer]::Vocab.Add('<|pad|>')
        [SimpleTokenizer]::Str2Id = @{}
        [SimpleTokenizer]::Id2Str = @{}
        $id = 0
        foreach ($tok in [SimpleTokenizer]::Vocab) {
            [SimpleTokenizer]::Str2Id[$tok] = $id
            [SimpleTokenizer]::Id2Str[$id] = $tok
            $id++
        }
        [SimpleTokenizer]::VocabSize = $id
    }

    static [int[]] Encode([string]$text) {
        $chars = [char[]]$text.ToLower().Trim()
        $seq = [System.Collections.Generic.List[string]]($chars | ForEach-Object { $_.ToString() })
        $ids = [System.Collections.Generic.List[int]]::new()
        while ($seq.Count -gt 0) {
            $found = $false
            for ($l = [math]::Min(8, $seq.Count); $l -gt 0; $l--) {
                $piece = -join $seq[0..($l - 1)]
                if ([SimpleTokenizer]::Str2Id.ContainsKey($piece)) {
                    $ids.Add([SimpleTokenizer]::Str2Id[$piece])
                    $seq.RemoveRange(0, $l)
                    $found = $true
                    break
                }
            }
            if (-not $found) {
                $ids.Add([SimpleTokenizer]::Str2Id['<|unk|>'])
                $seq.RemoveAt(0)
            }
        }
        return $ids.ToArray()
    }

    static [string] Decode([int[]]$ids) {
        $str = ($ids | ForEach-Object { [SimpleTokenizer]::Id2Str[$_] }) -join ''
        $str.Replace('<|endoftext|>', "`n")
    }
}
# endregion

# region --------------------------- tensor utils ---------------------------
function New-RandomMatrix {
    param([int]$Rows, [int]$Cols, [double]$Scale = 0.1)
    $m = New-Object 'double[,]' $Rows, $Cols
    $rand = [Random]::new()
    for ($r = 0; $r -lt $Rows; $r++) {
        for ($c = 0; $c -lt $Cols; $c++) {
            $m[$r, $c] = $Scale * ($rand.NextDouble() - 0.5)
        }
    }
    $m
}

function Multiply-MatrixVector {
    param([double[, ]]$M, [double[]]$V)
    $rows = $M.GetLength(0)
    $res = New-Object double[] $rows
    for ($r = 0; $r -lt $rows; $r++) {
        $sum = 0.0
        for ($c = 0; $c -lt $V.Count; $c++) { $sum += $M[$r, $c] * $V[$c] }
        $res[$r] = $sum
    }
    $res
}

function Add-Vectors { param([double[]]$A, [double[]]$B) 0..($A.Count - 1) | ForEach-Object { $A[$_] + $B[$_] } }
# endregion

# region --------------------------- Transformer ---------------------------
class TransformerBlock {
    [int]$EmbedDim
    [int]$Heads
    [double[, ]]$Wq
    [double[, ]]$Wk
    [double[, ]]$Wv
    [double[, ]]$Wo
    [double[, ]]$W1
    [double[, ]]$W2
    [double[]]$Ln1Gain
    [double[]]$Ln1Bias
    [double[]]$Ln2Gain
    [double[]]$Ln2Bias

    TransformerBlock([int]$EmbedDim, [int]$Heads) {
        $this.EmbedDim = $EmbedDim
        $this.Heads = $Heads
        $headDim = $EmbedDim / $Heads
        $this.Wq = New-RandomMatrix $EmbedDim $EmbedDim
        $this.Wk = New-RandomMatrix $EmbedDim $EmbedDim
        $this.Wv = New-RandomMatrix $EmbedDim $EmbedDim
        $this.Wo = New-RandomMatrix $EmbedDim $EmbedDim
        $hidden = 4 * $EmbedDim
        $this.W1 = New-RandomMatrix $hidden $EmbedDim
        $this.W2 = New-RandomMatrix $EmbedDim $hidden
        $this.Ln1Gain = 1..$EmbedDim | ForEach-Object { 1.0 }
        $this.Ln1Bias = 1..$EmbedDim | ForEach-Object { 0.0 }
        $this.Ln2Gain = 1..$EmbedDim | ForEach-Object { 1.0 }
        $this.Ln2Bias = 1..$EmbedDim | ForEach-Object { 0.0 }
    }

    [double[]] Forward([double[]]$x, [double[, ]]$mask) {
        $q = Multiply-MatrixVector $this.Wq $x
        $k = Multiply-MatrixVector $this.Wk $x
        $v = Multiply-MatrixVector $this.Wv $x
        $headDim = $this.EmbedDim / $this.Heads
        $attnOut = New-Object double[] $this.EmbedDim
        for ($h = 0; $h -lt $this.Heads; $h++) {
            $qH = $q[($h * $headDim)..(($h + 1) * $headDim - 1)]
            $kH = $k[($h * $headDim)..(($h + 1) * $headDim - 1)]
            $vH = $v[($h * $headDim)..(($h + 1) * $headDim - 1)]
            $scores = New-Object double[] $mask.GetLength(0)
            for ($t = 0; $t -lt $mask.GetLength(0); $t++) {
                $sum = 0.0
                for ($d = 0; $d -lt $headDim; $d++) { $sum += $qH[$d] * $kH[$d] }
                $scores[$t] = $sum / [math]::Sqrt($headDim) + $mask[$t, 0]
            }
            $probs = Get-Softmax $scores
            $oH = New-Object double[] $headDim
            for ($d = 0; $d -lt $headDim; $d++) {
                $sum = 0.0
                for ($t = 0; $t -lt $probs.Count; $t++) { $sum += $probs[$t] * $vH[$d] }
                $oH[$d] = $sum
            }
            for ($d = 0; $d -lt $headDim; $d++) { $attnOut[$h * $headDim + $d] = $oH[$d] }
        }
        $attnOut = Multiply-MatrixVector $this.Wo $attnOut
        $x = Add-Vectors $x $attnOut
        $norm = Get-LayerNorm $x
        for ($i = 0; $i -lt $norm.Count; $i++) { $norm[$i] = $norm[$i] * $this.Ln1Gain[$i] + $this.Ln1Bias[$i] }
        $x = $norm

        $ff = Multiply-MatrixVector $this.W1 $x
        $ff = $ff | ForEach-Object { Get-Gelu $_ }
        $ff = Multiply-MatrixVector $this.W2 $ff
        $x = Add-Vectors $x $ff
        $norm2 = Get-LayerNorm $x
        for ($i = 0; $i -lt $norm2.Count; $i++) { $norm2[$i] = $norm2[$i] * $this.Ln2Gain[$i] + $this.Ln2Bias[$i] }
        return $norm2
    }
}

class PoshGPT {
    [int]$VocabSize
    [int]$EmbedDim
    [int]$MaxSeqLen
    [int]$Layers
    [double[, ]]$TokenEmbedding
    [double[, ]]$PosEmbedding
    [TransformerBlock[]]$Blocks
    [double[, ]]$LmHead

    PoshGPT([int]$VocabSize, [int]$EmbedDim, [int]$MaxSeqLen, [int]$Layers, [int]$Heads) {
        $this.VocabSize = $VocabSize
        $this.EmbedDim = $EmbedDim
        $this.MaxSeqLen = $MaxSeqLen
        $this.Layers = $Layers
        $scale = 0.1
        $this.TokenEmbedding = New-RandomMatrix $VocabSize $EmbedDim $scale
        $this.PosEmbedding = New-RandomMatrix $MaxSeqLen $EmbedDim $scale
        $this.Blocks = 1..$Layers | ForEach-Object { [TransformerBlock]::new($EmbedDim, $Heads) }
        $this.LmHead = New-RandomMatrix $VocabSize $EmbedDim $scale
    }

    [double[]] Embed([int[]]$ids) {
        $seqLen = $ids.Count
        $x = New-Object double[] $this.EmbedDim
        for ($t = 0; $t -lt $seqLen; $t++) {
            $tokenRow = $ids[$t]
            for ($d = 0; $d -lt $this.EmbedDim; $d++) {
                $x[$d] += $this.TokenEmbedding[$tokenRow, $d] + $this.PosEmbedding[$t, $d]
            }
        }
        for ($d = 0; $d -lt $this.EmbedDim; $d++) { $x[$d] /= $seqLen }
        return $x
    }

    [double[]] Forward([int[]]$ids) {
        $x = $this.Embed($ids)
        $mask = New-Object 'double[,]' $ids.Count, 1
        foreach ($block in $this.Blocks) { $x = $block.Forward($x, $mask) }
        return $x
    }

    [int] SampleToken([int[]]$ids, [double]$Temperature, [int]$TopK) {
        if ($Temperature -le 0) { $Temperature = 1.0 }
        if ($TopK -le 0) { $TopK = 10 }
        $hidden = $this.Forward($ids)
        $logits = Multiply-MatrixVector $this.LmHead $hidden
        $top = $logits | Sort-Object -Descending | Select-Object -First $TopK
        $idx = 0..($logits.Count - 1) | Sort-Object { $logits[$_] } -Descending | Select-Object -First $TopK
        $probs = Get-Softmax $top $Temperature
        $r = Get-Random -Minimum 0.0 -Maximum 1.0
        $cum = 0.0
        for ($i = 0; $i -lt $probs.Count; $i++) {
            $cum += $probs[$i]
            if ($r -le $cum) { return $idx[$i] }
        }
        return $idx[-1]
    }
}
# endregion

# region ------------------------ training loop ---------------------------
function Invoke-PoshGPTTraining {
    param(
        [PoshGPT]$Model,
        [int[][]]$Dataset,
        [int]$Epochs = 20,
        [double]$Lr = 0.01,
        [scriptblock]$ProgressAction
    )
    for ($e = 0; $e -lt $Epochs; $e++) {
        $loss = 0.0
        foreach ($seq in $Dataset) {
            for ($i = 1; $i -lt $seq.Count; $i++) {
                $inp = $seq[0..($i - 1)]
                $target = $seq[$i]
                $logits = Multiply-MatrixVector $Model.LmHead ($Model.Forward($inp))
                $probs = Get-Softmax $logits
                $loss += - [math]::Log([math]::Max(1e-10, $probs[$target]))
                for ($v = 0; $v -lt $Model.VocabSize; $v++) {
                    $grad = $probs[$v] - ($v -eq $target ? 1.0 : 0.0)
                    $hidden = $Model.Forward($inp)
                    for ($d = 0; $d -lt $Model.EmbedDim; $d++) {
                        $Model.LmHead[$v, $d] -= $Lr * $grad * $hidden[$d]
                    }
                }
            }
        }
        Write-Host "epoch $($e+1)  loss=$(($loss/$Dataset.Count).ToString('0.000'))"
        if ($ProgressAction) {
            & $ProgressAction -Epoch ($e + 1) -Total $Epochs -Loss ($loss / $Dataset.Count)
        }
    }
}
# endregion

# region ------------------------ module surface ----------------------------
$Script:PoshGPT_Models = @{}

function Initialize-PoshLLM {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string[]]$Corpus,
        [int]$VocabSize = 300,
        [int]$EmbedDim = 64,
        [int]$MaxSeqLen = 128,
        [int]$Layers = 4,
        [int]$Heads = 4,
        [int]$Epochs = 20,
        [scriptblock]$ProgressAction,
        [switch]$Force
    )
    if ($Script:PoshGPT_Models.ContainsKey($Name) -and -not $Force) {
        throw "Model '$Name' already exists. Use -Force to overwrite."
    }
    Write-Host 'Building tokenizer...' -ForegroundColor Cyan
    [SimpleTokenizer]::Build($Corpus, $VocabSize)
    Write-Host "Vocab size: $([SimpleTokenizer]::VocabSize)" -ForegroundColor Green

    Write-Host 'Encoding dataset...'
    $dataset = $Corpus | ForEach-Object { [SimpleTokenizer]::Encode($_) }

    Write-Host 'Creating model...'
    $model = [PoshGPT]::new([SimpleTokenizer]::VocabSize, $EmbedDim, $MaxSeqLen, $Layers, $Heads)

    Write-Host 'Training... (CPU only)'
    Invoke-PoshGPTTraining -Model $model -Dataset $dataset -Epochs $Epochs -ProgressAction $ProgressAction

    $Script:PoshGPT_Models[$Name] = $model
    return $model
}

function Invoke-PoshLLM {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Name,
        [string]$Prompt = '',
        [int]$MaxTokens = 40,
        [double]$Temperature = 0.8,
        [int]$TopK = 10
    )
    if (-not $Script:PoshGPT_Models.ContainsKey($Name)) { throw "Model '$Name' not found." }
    $model = $Script:PoshGPT_Models[$Name]
    $ids = [SimpleTokenizer]::Encode($Prompt)
    $outTokens = [System.Collections.Generic.List[int]]::new($ids)
    for ($i = 0; $i -lt $MaxTokens; $i++) {
        $next = $model.SampleToken($outTokens.ToArray(), $Temperature, $TopK)
        if ([SimpleTokenizer]::Id2Str[$next] -eq '<|endoftext|>') { break }
        $outTokens.Add($next)
    }
    $text = [SimpleTokenizer]::Decode($outTokens.ToArray())
    [pscustomobject]@{ Model = $Name; Prompt = $Prompt; Output = $text }
}

function Save-PoshLLM {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Path
    )
    if (-not $Script:PoshGPT_Models.ContainsKey($Name)) { throw "Model '$Name' not found." }
    $model = $Script:PoshGPT_Models[$Name]
    $o = [pscustomobject]@{
        TokenEmbedding = $model.TokenEmbedding
        PosEmbedding   = $model.PosEmbedding
        LmHead         = $model.LmHead
        VocabSize      = $model.VocabSize
        EmbedDim       = $model.EmbedDim
        MaxSeqLen      = $model.MaxSeqLen
        Layers         = $model.Layers
        Vocab          = [SimpleTokenizer]::Vocab
        Str2Id         = [SimpleTokenizer]::Str2Id
        Id2Str         = [SimpleTokenizer]::Id2Str
    }
    $json = $o | ConvertTo-Json -Depth 6 -Compress
    [IO.File]::WriteAllText($Path, $json)
    return $Path
}

function Load-PoshLLM {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Path
    )
    $o = Get-Content $Path -Raw | ConvertFrom-Json
    [SimpleTokenizer]::Vocab = [System.Collections.Generic.List[string]]($o.Vocab)
    [SimpleTokenizer]::Str2Id = @{}
    [SimpleTokenizer]::Id2Str = @{}
    $o.Str2Id.PSObject.Properties | ForEach-Object { [SimpleTokenizer]::Str2Id[$_.Name] = $_.Value }
    $o.Id2Str.PSObject.Properties | ForEach-Object { [SimpleTokenizer]::Id2Str[[int]$_.Name] = $_.Value }
    [SimpleTokenizer]::VocabSize = $o.VocabSize

    $model = [PoshGPT]::new($o.VocabSize, $o.EmbedDim, $o.MaxSeqLen, $o.Layers, 4)
    $model.TokenEmbedding = $o.TokenEmbedding
    $model.PosEmbedding = $o.PosEmbedding
    $model.LmHead = $o.LmHead
    $Script:PoshGPT_Models[$Name] = $model
    return $model
}

Export-ModuleMember -Function Initialize-PoshLLM, Invoke-PoshLLM, Save-PoshLLM, Load-PoshLLM
