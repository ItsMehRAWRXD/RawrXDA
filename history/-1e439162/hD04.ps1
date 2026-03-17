<#
.SYNOPSIS
    PoshLLM CLI command handlers
.DESCRIPTION
    Handles poshllm-train, poshllm-generate, poshllm-list, and poshllm-save commands
#>

function Invoke-PoshLLMTrainHandler {
    param(
        [string]$Name,
        [string]$CorpusFile,
        [int]$VocabSize = 300,
        [int]$EmbedDim = 64,
        [int]$MaxSeqLen = 128,
        [int]$Layers = 4,
        [int]$Heads = 4,
        [int]$Epochs = 20
    )

    if (-not $Name) {
        Write-Host "Error: -Name parameter is required for poshllm-train command" -ForegroundColor Red
        return 1
    }

    if (-not $CorpusFile -or -not (Test-Path $CorpusFile)) {
        Write-Host "Error: -CorpusFile parameter is required and file must exist" -ForegroundColor Red
        return 1
    }

    try {
        Write-Host "Loading corpus from $CorpusFile..." -ForegroundColor Cyan
        $corpus = Get-Content $CorpusFile | Where-Object { $_ -ne '' }

        Write-Host "Training PoshLLM model '$Name' with $($corpus.Count) lines..." -ForegroundColor Cyan
        Write-Host "Parameters: VocabSize=$VocabSize, EmbedDim=$EmbedDim, Layers=$Layers, Heads=$Heads, Epochs=$Epochs" -ForegroundColor Gray

        $model = Initialize-PoshLLM -Name $Name -Corpus $corpus -VocabSize $VocabSize -EmbedDim $EmbedDim -MaxSeqLen $MaxSeqLen -Layers $Layers -Heads $Heads -Epochs $Epochs

        Write-Host "✅ Model '$Name' trained successfully!" -ForegroundColor Green

        return 0
    }
    catch {
        Write-Host "❌ Error training model: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

function Invoke-PoshLLMGenerateHandler {
    param(
        [string]$Name,
        [string]$Prompt = "",
        [int]$MaxTokens = 50,
        [double]$Temperature = 0.8,
        [int]$TopK = 10
    )

    if (-not $Name) {
        Write-Host "Error: -Name parameter is required for poshllm-generate command" -ForegroundColor Red
        return 1
    }

    try {
        Write-Host "Generating text with model '$Name'..." -ForegroundColor Cyan
        Write-Host "Prompt: '$Prompt'" -ForegroundColor Gray
        Write-Host "Max tokens: $MaxTokens, Temperature: $Temperature, TopK: $TopK" -ForegroundColor Gray
        Write-Host "" -ForegroundColor Gray

        $result = Invoke-PoshLLM -Name $Name -Prompt $Prompt -MaxTokens $MaxTokens -Temperature $Temperature -TopK $TopK

        Write-Host "Generated text:" -ForegroundColor Green
        Write-Host $result -ForegroundColor White
        Write-Host "" -ForegroundColor Gray

        return 0
    }
    catch {
        Write-Host "❌ Error generating text: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

function Invoke-PoshLLMListHandler {
    try {
        # Access the models from the PoshLLM module
        $models = $Script:PoshGPT_Models

        if ($models.Count -eq 0) {
            Write-Host "No PoshLLM models found. Use 'poshllm-train' to create one." -ForegroundColor Yellow
            return 0
        }

        Write-Host "Available PoshLLM models:" -ForegroundColor Cyan
        Write-Host "─" * 50 -ForegroundColor Gray

        foreach ($modelName in $models.Keys) {
            $model = $models[$modelName]
            Write-Host "🤖 $modelName" -ForegroundColor White
            Write-Host "   Vocab Size: $([SimpleTokenizer]::_VocabSize)" -ForegroundColor Gray
            Write-Host "   Embed Dim: $($model.EmbedDim)" -ForegroundColor Gray
            Write-Host "   Layers: $($model.Layers)" -ForegroundColor Gray
            Write-Host "   Heads: $($model.Heads)" -ForegroundColor Gray
            Write-Host "" -ForegroundColor Gray
        }

        return 0
    }
    catch {
        Write-Host "❌ Error listing models: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

function Invoke-PoshLLMSaveHandler {
    param(
        [string]$Name,
        [string]$Path
    )

    if (-not $Name) {
        Write-Host "Error: -Name parameter is required for poshllm-save command" -ForegroundColor Red
        return 1
    }

    if (-not $Path) {
        Write-Host "Error: -Path parameter is required for poshllm-save command" -ForegroundColor Red
        return 1
    }

    try {
        Write-Host "Saving model '$Name' to $Path..." -ForegroundColor Cyan
        Save-PoshLLM -Name $Name -Path $Path
        Write-Host "✅ Model saved successfully!" -ForegroundColor Green

        return 0
    }
    catch {
        Write-Host "❌ Error saving model: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

function Invoke-PoshLLMLoadHandler {
    param(
        [string]$Name,
        [string]$Path
    )

    if (-not $Name) {
        Write-Host "Error: -Name parameter is required for poshllm-load command" -ForegroundColor Red
        return 1
    }

    if (-not $Path -or -not (Test-Path $Path)) {
        Write-Host "Error: -Path parameter is required and file must exist" -ForegroundColor Red
        return 1
    }

    try {
        Write-Host "Loading model from $Path as '$Name'..." -ForegroundColor Cyan
        Load-PoshLLM -Name $Name -Path $Path
        Write-Host "✅ Model loaded successfully!" -ForegroundColor Green

        return 0
    }
    catch {
        Write-Host "❌ Error loading model: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
# Functions exported:
#   'Invoke-PoshLLMTrainHandler',
#   'Invoke-PoshLLMGenerateHandler',
#   'Invoke-PoshLLMListHandler',
#   'Invoke-PoshLLMSaveHandler',
#   'Invoke-PoshLLMLoadHandler'