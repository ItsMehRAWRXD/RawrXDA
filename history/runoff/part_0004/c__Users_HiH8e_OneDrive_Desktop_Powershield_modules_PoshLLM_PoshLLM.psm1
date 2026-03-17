<#
PoshLLM.psm1 – Mini GPT with parser-safe pattern (fields-only class, functions as methods).
This avoids PowerShell class parser edge-cases by keeping methods as plain functions.
#>

# ---------------- math helpers ----------------
function PL-Gelu([double]$x) { $x3 = $x * $x * $x; 0.5 * $x * (1.0 + [math]::Tanh(0.7978845608 * ($x + 0.044715 * $x3))) }
function PL-Softmax([double[]]$x, [double]$Temp = 1.0) {
  $max = ($x | Measure-Object -Maximum).Maximum
  $exp = $x | ForEach-Object { [math]::Exp(($_ - $max) / $Temp) }
  $sum = ($exp | Measure-Object -Sum).Sum
  $exp | ForEach-Object { $_ / $sum }
}
function PL-LayerNorm([double[]]$x, [double]$Eps = 1e-5) {
  $mean = ($x | Measure-Object -Average).Average
  $var = ($x | ForEach-Object { ($_ - $mean) * ($_ - $mean) } | Measure-Object -Average).Average
  $x | ForEach-Object { ($_ - $mean) / [math]::Sqrt($var + $Eps) }
}
function PL-RandomMatrix([int]$Rows, [int]$Cols, [double]$Scale = 0.02) {
  # Use Array.CreateInstance to guarantee a true 2D array (System.Double[,])
  $m = [Array]::CreateInstance([double], $Rows, $Cols)
  $rng = [Random]::new()
  for ($r = 0; $r -lt $Rows; $r++) { for ($c = 0; $c -lt $Cols; $c++) { $m.SetValue(($Scale * ($rng.NextDouble() - 0.5)), $r, $c) } }
  # Cast explicitly for clarity
  return $m
}

# Strict 2-D weight factory (explicit cast)
function New-WeightMatrix {
  param([int]$Rows, [int]$Cols, [double]$Scale = 0.01)
  $a = [Array]::CreateInstance([double], $Rows, $Cols)
  for ($r = 0; $r -lt $Rows; $r++) {
    for ($c = 0; $c -lt $Cols; $c++) {
      $a[$r, $c] = (Get-Random -Minimum -1.0 -Maximum 1.0) * $Scale
    }
  }
  return $a
}
function PL-MatVec([double[,]]$M, [double[]]$v) {
  $rows = $M.GetLength(0); $cols = $M.GetLength(1)
  $y = New-Object double[] $rows
  for ($r = 0; $r -lt $rows; $r++) { $s = 0.0; for ($c = 0; $c -lt $cols; $c++) { $s += $M[$r, $c] * $v[$c] }; $y[$r] = $s }
  $y
}
function PL-Add([double[]]$a, [double[]]$b) { 0..($a.Count - 1) | ForEach-Object { $a[$_] + $b[$_] } }

# ---------------- tokenizer (BPE-ish) ----------------
$Script:PL_Tokenizer = @{ Vocab = @(); Str2Id = @{}; Id2Str = @{}; VocabSize = 0 }
function Build-Tokenizer([string[]]$Corpus, [int]$Merges = 300) {
  $all = New-Object System.Collections.Generic.List[string]
  foreach ($line in $Corpus) { $line = $line.ToLower().Trim(); if ($line) { [char[]]$line | ForEach-Object { $all.Add($_.ToString()) }; $all.Add('<|endoftext|>') } }
  $pairs = @{}; for ($i = 0; $i -lt $all.Count - 1; $i++) { $pair = $all[$i] + '‖' + $all[$i + 1]; $pairs[$pair] = $pairs[$pair] + 1 }
  $merged = [System.Collections.Generic.List[string]]::new($all)
  for ($m = 0; $m -lt $Merges; $m++) {
    $best = ($pairs.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1).Name; if (-not $best) { break }
    $a, $b = $best -split '‖'; $i = 0; while ($i -lt $merged.Count - 1) { if ($merged[$i] -ceq $a -and $merged[$i + 1] -ceq $b) { $merged[$i] = $a + $b; $merged.RemoveAt($i + 1) } else { $i++ } }
    $pairs.Clear(); for ($i = 0; $i -lt $merged.Count - 1; $i++) { $pair = $merged[$i] + '‖' + $merged[$i + 1]; $pairs[$pair] = $pairs[$pair] + 1 }
  }
  $vset = [System.Collections.Generic.HashSet[string]]::new([string[]]$merged)
  $vset.Add('<|unk|>') | Out-Null; $vset.Add('<|pad|>') | Out-Null
  $Script:PL_Tokenizer.Vocab = @($vset)
  $Script:PL_Tokenizer.Str2Id = @{}; $Script:PL_Tokenizer.Id2Str = @{}
  $id = 0; foreach ($t in $Script:PL_Tokenizer.Vocab) { $Script:PL_Tokenizer.Str2Id[$t] = $id; $Script:PL_Tokenizer.Id2Str[$id] = $t; $id++ }
  $Script:PL_Tokenizer.VocabSize = $id
}
function Encode-Text([string]$text) {
  $text = ($text ?? '').ToLower().Trim()
  $seq = [System.Collections.Generic.List[string]]::new(); [char[]]$text | ForEach-Object { $seq.Add($_.ToString()) }
  $ids = [System.Collections.Generic.List[int]]::new()
  while ($seq.Count -gt 0) {
    $found = $false; for ($l = [math]::Min(8, $seq.Count); $l -gt 0; $l--) { $piece = -join $seq[0..($l - 1)]; if ($Script:PL_Tokenizer.Str2Id.ContainsKey($piece)) { $ids.Add($Script:PL_Tokenizer.Str2Id[$piece]); $seq.RemoveRange(0, $l); $found = $true; break } }
    if (-not $found) { $ids.Add($Script:PL_Tokenizer.Str2Id['<|unk|>']); $seq.RemoveAt(0) } 
  }
  [int[]]$ids.ToArray()
}
function Decode-Ids([int[]]$ids) { (($ids | ForEach-Object { $Script:PL_Tokenizer.Id2Str[$_] }) -join '').Replace('<|endoftext|>', "`n") }

# ---------------- class with fields only ----------------
class MiniGPT {
  [int]        $VocabSize
  [int]        $EmbedDim
  [int]        $NumHeads
  [int]        $NumLayers
  [int]        $MaxPos
    [object]     $TokenEmbedding  # stored as System.Double[,] internally
    [object]     $PosEmbedding    # stored as System.Double[,] internally
  [object[]]   $AttnWq
  [object[]]   $AttnWk
  [object[]]   $AttnWv
  [object[]]   $AttnWo
  [object[]]   $FfnW1
  [object[]]   $FfnW2
  [object[]]   $Ln1Gain
  [object[]]   $Ln1Bias
  [object[]]   $Ln2Gain
  [object[]]   $Ln2Bias
    [object]     $LmHead          # stored as System.Double[,] internally
  [object]     $Tokenizer

  MiniGPT([int]$vocab, [int]$dim, [int]$heads, [int]$layers, [int]$maxPos) {
    $this.VocabSize = $vocab; $this.EmbedDim = $dim; $this.NumHeads = $heads; $this.NumLayers = $layers; $this.MaxPos = $maxPos
    Initialize-Weights $this
  }
}

# ---------------- former methods as functions ----------------
function Initialize-Weights([MiniGPT]$g) {
  # Use strict 2-D factory to avoid any casting issues
  $g.TokenEmbedding = New-WeightMatrix -Rows $g.VocabSize -Cols $g.EmbedDim -Scale 0.02
  $g.PosEmbedding = New-WeightMatrix -Rows $g.MaxPos   -Cols $g.EmbedDim -Scale 0.02
  $g.AttnWq = @(); $g.AttnWk = @(); $g.AttnWv = @(); $g.AttnWo = @(); $g.FfnW1 = @(); $g.FfnW2 = @(); $g.Ln1Gain = @(); $g.Ln1Bias = @(); $g.Ln2Gain = @(); $g.Ln2Bias = @()
  for ($l = 0; $l -lt $g.NumLayers; $l++) {
    $g.AttnWq += , (New-WeightMatrix -Rows $g.EmbedDim -Cols $g.EmbedDim -Scale 0.02)
    $g.AttnWk += , (New-WeightMatrix -Rows $g.EmbedDim -Cols $g.EmbedDim -Scale 0.02)
    $g.AttnWv += , (New-WeightMatrix -Rows $g.EmbedDim -Cols $g.EmbedDim -Scale 0.02)
    $g.AttnWo += , (New-WeightMatrix -Rows $g.EmbedDim -Cols $g.EmbedDim -Scale 0.02)
    $g.FfnW1 += , (New-WeightMatrix -Rows (4 * $g.EmbedDim) -Cols $g.EmbedDim -Scale 0.02)
    $g.FfnW2 += , (New-WeightMatrix -Rows $g.EmbedDim -Cols (4 * $g.EmbedDim) -Scale 0.02)
    $g.Ln1Gain += , (@(for ($i = 0; $i -lt $g.EmbedDim; $i++) { 1.0 }))
    $g.Ln1Bias += , (@(for ($i = 0; $i -lt $g.EmbedDim; $i++) { 0.0 }))
    $g.Ln2Gain += , (@(for ($i = 0; $i -lt $g.EmbedDim; $i++) { 1.0 }))
    $g.Ln2Bias += , (@(for ($i = 0; $i -lt $g.EmbedDim; $i++) { 0.0 }))
  }
  # LmHead: rows=vocab, cols=embedDim -> logits = LmHead @ hidden
  $g.LmHead = New-WeightMatrix -Rows $g.VocabSize -Cols $g.EmbedDim -Scale 0.02
}

function Embed-Sequence([MiniGPT]$g, [int[]]$ids) {
  $L = $ids.Count; if ($L -eq 0) { return @(for ($i = 0; $i -lt $g.EmbedDim; $i++) { 0.0 }) }
  $x = New-Object double[] $g.EmbedDim
  for ($t = 0; $t -lt $L; $t++) {
    $row = $ids[$t]; for ($d = 0; $d -lt $g.EmbedDim; $d++) { $x[$d] += $g.TokenEmbedding[$row, $d] + $g.PosEmbedding[[math]::Min($t, $g.MaxPos - 1), $d] }
  }
  for ($d = 0; $d -lt $g.EmbedDim; $d++) { $x[$d] /= $L }
  $x
}

function Forward([MiniGPT]$g, [int[]]$ids) {
  $x = Embed-Sequence $g $ids
  for ($l = 0; $l -lt $g.NumLayers; $l++) {
    $q = PL-MatVec ($g.AttnWq[$l]) $x; $k = PL-MatVec ($g.AttnWk[$l]) $x; $v = PL-MatVec ($g.AttnWv[$l]) $x
    # toy attention: dot(q,k) as scalar weight
    $dot = 0.0; for ($i = 0; $i -lt $g.EmbedDim; $i++) { $dot += $q[$i] * $k[$i] }
    $scale = $dot / [math]::Sqrt($g.EmbedDim)
    $attn = @(); for ($i = 0; $i -lt $g.EmbedDim; $i++) { $attn += , ($scale * $v[$i]) }
    $attnProj = PL-MatVec ($g.AttnWo[$l]) $attn
    $x = PL-Add $x $attnProj
    $x = PL-LayerNorm $x
    $ff = PL-MatVec ($g.FfnW1[$l]) $x; $ff = $ff | ForEach-Object { PL-Gelu $_ }; $ff = PL-MatVec ($g.FfnW2[$l]) $ff
    $x = PL-Add $x $ff
    $x = PL-LayerNorm $x
  }
  $x
}

function Sample-Token([MiniGPT]$g, [int[]]$ids, [double]$Temperature = 1.0, [int]$TopK = 10) {
  if ($Temperature -le 0) { $Temperature = 1.0 }
  if ($TopK -le 0) { $TopK = 10 }
  $hidden = Forward $g $ids
  $logits = PL-MatVec $g.LmHead $hidden
  $top = $logits | Sort-Object -Descending | Select-Object -First $TopK
  $idx = 0..($logits.Count - 1) | Sort-Object { $logits[$_] } -Descending | Select-Object -First $TopK
  $probs = PL-Softmax $top $Temperature
  $r = Get-Random -Minimum 0.0 -Maximum 1.0; $cum = 0.0
  for ($i = 0; $i -lt $probs.Count; $i++) { $cum += $probs[$i]; if ($r -le $cum) { return $idx[$i] } }
  $idx[-1]
}

function Generate([MiniGPT]$g, [string]$prompt, [int]$maxNew = 20, [double]$temp = 1.0, [int]$topK = 10) {
  $ids = Encode-Text $prompt
  $out = New-Object 'System.Collections.Generic.List[int]'
  foreach ($id in [int[]]$ids) { [void]$out.Add([int]$id) }
  for ($i = 0; $i -lt $maxNew; $i++) {
    $next = Sample-Token $g $out.ToArray() $temp $topK
    if ($Script:PL_Tokenizer.Id2Str[$next] -eq '<|endoftext|>') { break }
    $out.Add($next)
  }
  Decode-Ids $out.ToArray()
}

function Train-OneEpoch([MiniGPT]$g, [string[]]$Corpus, [int]$Epochs = 1, [double]$Lr = 0.01) {
  $dataset = $Corpus | ForEach-Object { Encode-Text $_ }
  for ($e = 0; $e -lt $Epochs; $e++) {
    $loss = 0.0
    foreach ($seq in $dataset) {
      for ($i = 1; $i -lt $seq.Count; $i++) {
        $inp = $seq[0..($i - 1)]; $target = $seq[$i]
        $hidden = Forward $g $inp
        $logits = PL-MatVec $g.LmHead $hidden
        $probs = PL-Softmax $logits 1.0
        $p = [math]::Max(1e-10, $probs[$target]); $loss += - [math]::Log($p)
        for ($v = 0; $v -lt $g.VocabSize; $v++) {
          $grad = $probs[$v] - ([int]($v -eq $target))
          for ($d = 0; $d -lt $g.EmbedDim; $d++) { $g.LmHead[$v, $d] -= $Lr * $grad * $hidden[$d] }
        }
      } 
    }
    Write-Host ("epoch {0} loss={1}" -f ($e + 1), ($loss / [math]::Max(1, $dataset.Count)).ToString('0.000'))
  }
}

# ---------------- module surface ----------------
$Script:PoshLLM_Models = @{}

function Initialize-PoshLLM {
  [CmdletBinding()] param(
    [Parameter(Mandatory)][string]$Name,
    [Parameter(Mandatory)][string[]]$Corpus,
    [int]$VocabSize = 300,
    [int]$EmbedDim = 64,
    [int]$MaxSeqLen = 128,
    [int]$Layers = 2,
    [int]$Heads = 4,
    [int]$Epochs = 5,
    [switch]$Force
  )
  if ($Script:PoshLLM_Models.ContainsKey($Name) -and -not $Force) { throw "Model '$Name' already exists. Use -Force to overwrite." }
  Build-Tokenizer -Corpus $Corpus -Merges $VocabSize
  $g = [MiniGPT]::new($Script:PL_Tokenizer.VocabSize, $EmbedDim, $Heads, $Layers, $MaxSeqLen)
  $g.Tokenizer = $Script:PL_Tokenizer
  Train-OneEpoch $g $Corpus $Epochs 0.01
  $Script:PoshLLM_Models[$Name] = $g
  $g
}

function Invoke-PoshLLM {
  [CmdletBinding()] param(
    [Parameter(Mandatory)][string]$Name,
    [string]$Prompt = '',
    [int]$MaxTokens = 40,
    [double]$Temperature = 0.8,
    [int]$TopK = 10
  )
  if (-not $Script:PoshLLM_Models.ContainsKey($Name)) { throw "Model '$Name' not found." }
  $g = $Script:PoshLLM_Models[$Name]
  $out = Generate $g $Prompt $MaxTokens $Temperature $TopK
  [pscustomobject]@{ Model = $Name; Prompt = $Prompt; Output = $out }
}

function Save-PoshLLM {
  [CmdletBinding()] param(
    [Parameter(Mandatory)][string]$Name,
    [Parameter(Mandatory)][string]$Path
  )
  if (-not $Script:PoshLLM_Models.ContainsKey($Name)) { throw "Model '$Name' not found." }
  $g = $Script:PoshLLM_Models[$Name]
  $o = [pscustomobject]@{
    VocabSize = $g.VocabSize; EmbedDim = $g.EmbedDim; NumHeads = $g.NumHeads; NumLayers = $g.NumLayers; MaxPos = $g.MaxPos
    TokenEmbedding = $g.TokenEmbedding; PosEmbedding = $g.PosEmbedding; LmHead = $g.LmHead
    AttnWq = $g.AttnWq; AttnWk = $g.AttnWk; AttnWv = $g.AttnWv; AttnWo = $g.AttnWo; FfnW1 = $g.FfnW1; FfnW2 = $g.FfnW2
    Ln1Gain = $g.Ln1Gain; Ln1Bias = $g.Ln1Bias; Ln2Gain = $g.Ln2Gain; Ln2Bias = $g.Ln2Bias
    Tokenizer = $Script:PL_Tokenizer
  }
  $json = $o | ConvertTo-Json -Depth 7 -Compress
  [IO.File]::WriteAllText($Path, $json)
  $Path
}

function Load-PoshLLM {
  [CmdletBinding()] param(
    [Parameter(Mandatory)][string]$Name,
    [Parameter(Mandatory)][string]$Path
  )
  $o = Get-Content -Raw -Path $Path | ConvertFrom-Json
  $g = [MiniGPT]::new([int]$o.VocabSize, [int]$o.EmbedDim, [int]$o.NumHeads, [int]$o.NumLayers, [int]$o.MaxPos)
  $g.TokenEmbedding = $o.TokenEmbedding; $g.PosEmbedding = $o.PosEmbedding; $g.LmHead = $o.LmHead
  $g.AttnWq = $o.AttnWq; $g.AttnWk = $o.AttnWk; $g.AttnWv = $o.AttnWv; $g.AttnWo = $o.AttnWo; $g.FfnW1 = $o.FfnW1; $g.FfnW2 = $o.FfnW2
  $g.Ln1Gain = $o.Ln1Gain; $g.Ln1Bias = $o.Ln1Bias; $g.Ln2Gain = $o.Ln2Gain; $g.Ln2Bias = $o.Ln2Bias
  $Script:PL_Tokenizer = $o.Tokenizer
  $Script:PoshLLM_Models[$Name] = $g
  $g
}

Export-ModuleMember -Function Initialize-PoshLLM, Invoke-PoshLLM, Save-PoshLLM, Load-PoshLLM
