<# Embed.psm1
   Embedding helper: tries Ollama CLI, fallback deterministic SHA256->vector
#>

function Get-DeterministicEmbedding {
    param(
        [Parameter(Mandatory)][string]$Text,
        [int]$Dim = 128
    )
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $bytes = $sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Text))
    # expand or repeat bytes to reach desired dimension
    $vec = New-Object 'System.Collections.Generic.List[double]'
    while ($vec.Count -lt $Dim) {
        foreach ($b in $bytes) {
            if ($vec.Count -ge $Dim) { break }
            # map byte (0..255) to -1..1
            $v = ($b / 255.0) * 2.0 - 1.0
            $vec.Add([double]$v)
        }
    }
    return ,($vec.ToArray())
}

function Try-OllamaEmbedding {
    param(
        [string]$Text,
        [string]$Model = 'mosaicml/mpt-7b-embed' # example
    )
    try {
        $cmd = Get-Command ollama -ErrorAction SilentlyContinue
        if (-not $cmd) { return $null }
        # Example: `ollama embed MODEL --text 'some text' --json`
        $args = @('embed', $Model, '--text', $Text, '--json')
        $proc = & ollama @args 2>&1
        $out = $proc -join "`n"
        # try to parse JSON array in output
        try { $json = $out | ConvertFrom-Json -ErrorAction Stop; return $json.embedding } catch { return $null }
    } catch { return $null }
}

function Get-Embedding {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Text,
        [int]$Dim = 128,
        [string]$Model = 'mosaicml/mpt-7b-embed'
    )
    # try Ollama first
    $emb = Try-OllamaEmbedding -Text $Text -Model $Model
    if ($emb) {
        # ensure length
        if ($emb.Count -lt $Dim) { $emb = $emb + (Get-DeterministicEmbedding -Text $Text -Dim ($Dim - $emb.Count)) }
        return $emb[0..($Dim - 1)]
    }

    # fallback deterministic
    return Get-DeterministicEmbedding -Text $Text -Dim $Dim
}

Export-ModuleMember -Function Get-Embedding, Get-DeterministicEmbedding, Try-OllamaEmbedding
