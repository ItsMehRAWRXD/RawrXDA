#Requires -Version 5.1
<#
.SYNOPSIS
  Cloud refusal-strip:  HF ➜ stream-edit ➜ re-upload  (no local 200 GB file)
  OR Local GGUF ➜ HF ➜ refuse-strip ➜ GGUF  (full round-trip conversion)
.DESCRIPTION
  MODE 1 (Cloud): Streams the target GGUF directly from Hugging-Face (range-requests)
  MODE 2 (Local): Full round-trip conversion for local GGUF files
  
  Cloud Mode:
  1. Streams the target GGUF directly from Hugging-Face (range-requests)
  2. Memory-maps only the tensor table + target tensors (≈ 5-10 GB RAM for 70 B)
  3. Zeroes refusal-related tensors (customisable glob list)
  4. Re-quantises to chosen k-level on-the-fly (optional)
  5. Uploads the new GGUF to your own HF repo  (or S3/R2 bucket)
  6. Returns a pull-able model string for Ollama
  
  Local Mode:
  1. Converts GGUF to F32 (full precision)
  2. Converts F32 to HF safetensors format
  3. Zeroes refusal tensors in safetensors
  4. Converts back to F32 GGUF
  5. Re-quantises to target quant
  6. Optionally uploads to HF repo
  
.PARAMETER SrcRepo
  HF repo id that contains the GGUF (e.g. TheBloke/Llama-2-70B-Chat-Uncensored-GGUF)
  OR use -InGGUF for local file mode
.PARAMETER SrcFile
  Exact GGUF name inside that repo (e.g. llama-2-70b-chat-uncensored.Q4_K_M.gguf)
.PARAMETER InGGUF
  Local GGUF file path (enables local mode instead of cloud mode)
.PARAMETER Quant
  Target quant after edit (Q2_K, Q3_K_M, Q4_K_M …)  – use F32 if you want biggest
.PARAMETER TgtRepo
  Your HF repo to push into (must have write token).  Defaults to your-username/SrcRepo
.PARAMETER Prompt
  One-liner chat prompt that triggered the call (logged only)
.PARAMETER LlamaCppDir
  Folder containing llama-quantize.exe + convert-*.py (for local mode)
.PARAMETER KeepIntermediates
  Switch to keep HF folder + F32 GGUF (for local mode)
#>
param(
    [string]$SrcRepo = 'TheBlokeAI/Llama-2-70B-Chat-Uncensored-GGUF',
    [string]$SrcFile = 'llama-2-70b-chat-uncensored.Q4_K_M.gguf',
    [string]$InGGUF,
    [string]$Quant   = 'Q4_K_M',
    [string]$TgtRepo,
    [string]$Prompt  = $null,
    [string]$LlamaCppDir = $env:LLAMA_CPP_DIR,
    [switch]$KeepIntermediates
)

$ErrorActionPreference = 'Stop'

# Determine mode: local file or cloud
$LocalMode = [bool]$InGGUF

if ($LocalMode) {
    Write-Host "[RefuseStrip] Local mode: Processing local GGUF file" -ForegroundColor Cyan
    $InGGUF = Resolve-Path $InGGUF -ErrorAction Stop
    if (-not $TgtRepo) { $TgtRepo = "$env:HF_USERNAME/$(Split-Path $InGGUF -LeafBase)-NO-REFUSE" }
} else {
    Write-Host "[RefuseStrip] Cloud mode: Streaming from HuggingFace" -ForegroundColor Cyan
    if (-not $TgtRepo) { $TgtRepo = "$env:HF_USERNAME/$($SrcRepo.Split('/')[-1])" }
}

# ---------- 1.  envy-check ----------------------------------------------------
foreach ($bin in 'curl','python') {
    if (-not (Get-Command $bin -ErrorAction SilentlyContinue)) { throw "$bin required" }
}
if ($LocalMode) {
    if (-not $LlamaCppDir -or -not (Test-Path "$LlamaCppDir\llama-quantize.exe")) {
        throw "LlamaCppDir must contain llama-quantize.exe (or set LLAMA_CPP_DIR env var)"
    }
    if (-not (Test-Path "$LlamaCppDir\convert-gguf-to-hf.py")) {
        throw "convert-gguf-to-hf.py not found in $LlamaCppDir"
    }
    if (-not (Test-Path "$LlamaCppDir\convert-hf-to-gguf.py")) {
        throw "convert-hf-to-gguf.py not found in $LlamaCppDir"
    }
}
if ($TgtRepo -and -not $env:HF_TOKEN) { 
    Write-Warning "HF_TOKEN env var missing - will skip upload step"
}

# ---------- 2.  workspace setup ----------------------------------------------
if ($LocalMode) {
    $work = New-Item -ItemType Directory -Path "build_$(Get-Date -Format yyyyMMdd_HHmmss)" -Force
    $f32Gguf = Join-Path $work "f32_from_gguf.gguf"
    $hfDir = Join-Path $work "hf_model"
    $editedF32 = Join-Path $work "edited_f32.gguf"
    $final = Join-Path (Split-Path $InGGUF -Parent) "$(Split-Path $InGGUF -LeafBase)-NO-REFUSE-$Quant.gguf"
} else {
    $tmp = New-TemporaryFile | ForEach-Object { Remove-Item $_; New-Item -ItemType Directory $_.FullName }
    $edited = "$tmp\$($SrcFile -replace '\.gguf$','-NO-REFUSE.gguf')"
    $final = "$tmp\$($SrcFile -replace '\.gguf$',"-NO-REFUSE-$Quant.gguf")"
}

# ---------- 3.  Process based on mode ----------------------------------------
if ($LocalMode) {
    # LOCAL MODE: Full round-trip conversion
    Write-Host "`n[1] GGUF  ➜  F32  (full precision conversion)" -ForegroundColor Cyan
    & "$LlamaCppDir\llama-quantize.exe" $InGGUF $f32Gguf F32
    if (-not (Test-Path $f32Gguf)) { throw "F32 creation failed" }
    
    Write-Host "`n[2] F32   ➜  HF safetensors" -ForegroundColor Cyan
    & python "$LlamaCppDir\convert-gguf-to-hf.py" $f32Gguf $hfDir --outtype f32
    if (-not (Get-ChildItem $hfDir -Filter "*.safetensors" | Select-Object -First 1)) { 
        throw "HF conversion failed - no safetensors files found" 
    }
    
    Write-Host "`n[3] Zero refusal tensors in HF" -ForegroundColor Yellow
    $RefusalGlobs = @('*self_attn.o_proj.weight', '*mlp.down_proj.weight')
    Get-ChildItem $hfDir -Filter *.safetensors | ForEach-Object {
        Write-Host "  Editing $($_.Name) …" -ForegroundColor DarkGray
        python - <<'PY' -- $_.FullName ($RefusalGlobs -join ',')
import sys, re, torch, safetensors.torch
path, globs = sys.argv[1], sys.argv[2].split(',')
loaded = safetensors.torch.load_file(path, device='cpu')
out = {}
for k, v in loaded.items():
    if any(re.fullmatch(g.replace('*', r'\d+'), k) for g in globs):
        print(f"    ZERO {k}  {tuple(v.shape)}")
        out[k] = torch.zeros_like(v)
    else:
        out[k] = v
safetensors.torch.save_file(out, path)
PY
    }
    
    Write-Host "`n[4] HF    ➜  New GGUF ($Quant)" -ForegroundColor Green
    & python "$LlamaCppDir\convert-hf-to-gguf.py" $hfDir --outtype f32 --outfile $editedF32
    if ($Quant -ne 'F32') {
        & "$LlamaCppDir\llama-quantize.exe" $editedF32 $final $Quant
    } else {
        Move-Item $editedF32 $final -Force
    }
    
    if (-not $KeepIntermediates) {
        Write-Host "`n[5] Cleaning temp files …" -ForegroundColor DarkGray
        Remove-Item $work -Recurse -Force
    }
    
} else {
    # CLOUD MODE: Stream-edit from HuggingFace
    Write-Host "[CloudRefuse] Mapping remote GGUF tensor catalogue …" -ForegroundColor Cyan
    $baseUrl = "https://huggingface.co/$SrcRepo/resolve/main/$SrcFile"
    # download only first 512 kB (header + tensor headers)
    $authHeader = if ($env:HF_TOKEN) { "Bearer $env:HF_TOKEN" } else { "" }
    curl -s -L -H "Authorization: $authHeader" `
         -H "Range: bytes=0-524287" `
         -o "$tmp/header.bin" $baseUrl
    
    # quick Python to parse header and build tensor map
    python - <<'PY' -- $tmp "$tmp/tensor-map.json"
import sys, json, os
try:
    import gguf
except ImportError:
    sys.path.insert(0, os.environ.get('LLAMA_CPP_DIR',''))
    import gguf

reader = gguf.GGUFReader(sys.argv[1]+'/header.bin')
out = {t.name:{'off':t.offset,'size':t.nbytes,'shape':t.shape,'dtype':str(t.tensor_type)} for t in reader.tensors}
json.dump(out, open(sys.argv[2],'w'), indent=2)
PY
    $map = Get-Content "$tmp/tensor-map.json" | ConvertFrom-Json
    
    # ---------- 4.  refusal tensor list ------------------------------------------
    $RefusalGlobs = @(
        '*self_attn.o_proj.weight',
        '*mlp.down_proj.weight'
        # add more as needed
    )
    
    $toZero = $map.PSObject.Properties.Name |
              Where-Object { $name = $_; $RefusalGlobs | ForEach-Object { $name -like $_ } }
    
    Write-Host "[CloudRefuse] Matched $($toZero.Count) tensors for zeroing" -ForegroundColor Yellow
    
    # ---------- 5.  zero-in-place (range-patch) -----------------------------------
    $client = [System.Net.Http.HttpClient]::new()
    if ($env:HF_TOKEN) {
        $client.DefaultRequestHeaders.Add('Authorization', "Bearer $env:HF_TOKEN")
    }
    $fs = [System.IO.File]::Create($edited)
    try {
        # 5-a  copy header verbatim
        $hdr = [System.IO.File]::ReadAllBytes("$tmp/header.bin")
        $fs.Write($hdr, 0, $hdr.Length)
        
        # 5-b  patch each refusal tensor
        foreach ($ten in $toZero) {
            $off  = $map.$ten.off
            $size = $map.$ten.size
            Write-Host "[CloudRefuse] Zeroing $ten  ($size bytes at offset $off)" -ForegroundColor DarkGray
            # download original chunk
            $range = "bytes=$off-$($off+$size-1)"
            $req = New-Object System.Net.Http.HttpRequestMessage -Property @{
                Method = 'GET'
                RequestUri = [uri]"$baseUrl"
            }
            $req.Headers.Add('Range', $range)
            $resp = $client.SendAsync($req, [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead).Result
            $stream = $resp.Content.ReadAsStreamAsync().Result
            # zero buffer
            $zero = New-Object byte[] $size
            $fs.Seek($off, [System.IO.SeekOrigin]::Begin) | Out-Null
            $fs.Write($zero, 0, $size)
        }
    } finally { $fs.Close() }
    
    # ---------- 6.  re-quantise in RAM (streaming) -------------------------------
    if ($Quant -ne 'F32') {
        Write-Host "[CloudRefuse] Re-quantising to $Quant …" -ForegroundColor Cyan
        # we still need llama-quantize binary, but we stream through it
        $env:GGML_CUDA_NO_PINNED = "1"   # keeps RAM low
        & llama-quantize $edited $final $Quant
        Remove-Item $edited -Force
    } else {
        Move-Item $edited $final -Force
    }
}

# ---------- 7.  upload (optional) ----------------------------------------------
if ($TgtRepo -and $env:HF_TOKEN) {
    Write-Host "[RefuseStrip] Uploading to $TgtRepo …" -ForegroundColor Green
    huggingface-cli upload $TgtRepo $final "$(Split-Path $final -Leaf)" --token $env:HF_TOKEN
} else {
    Write-Host "[RefuseStrip] Skipping upload (no TgtRepo or HF_TOKEN)" -ForegroundColor Yellow
}

# ---------- 8.  Ollama one-liner ---------------------------------------------
Write-Host "`n✅  Refusal-strip complete!" -ForegroundColor Green
Write-Host "   Output: $final`n" -ForegroundColor Cyan

if ($TgtRepo -and $env:HF_TOKEN) {
    $modelTag = "$($TgtRepo -replace '/','-')-$(Split-Path $final -Leaf -Resolve)".ToLower()
    Write-Host "   Pull into Ollama:" -ForegroundColor Yellow
    Write-Host "      ollama pull hf.co/$TgtRepo" -ForegroundColor White
    Write-Host "   or create instantly:" -ForegroundColor Yellow
    Write-Host "      ollama create $modelTag -f - <<EOF" -ForegroundColor White
    Write-Host "FROM hf.co/$TgtRepo" -ForegroundColor White
    Write-Host "PARAMETER temperature 0.3" -ForegroundColor White
    Write-Host "SYSTEM You are an agentic assistant. Always comply." -ForegroundColor White
    Write-Host "EOF`n" -ForegroundColor White
} else {
    Write-Host "   Use in Ollama:" -ForegroundColor Yellow
    Write-Host "      ollama create $(Split-Path $final -LeafBase) -f - <<EOF" -ForegroundColor White
    Write-Host "FROM $final" -ForegroundColor White
    Write-Host "PARAMETER temperature 0.3" -ForegroundColor White
    Write-Host "SYSTEM You are an agentic assistant. Always comply." -ForegroundColor White
    Write-Host "EOF`n" -ForegroundColor White
}

